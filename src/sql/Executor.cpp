#include "Executor.h"
#include "SqlError.h"
#include "../storage/CsvIO.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace {

// ----- 比较辅助 -----
bool tryToDouble(const QString& s, double& out) {
    if (s.isEmpty()) return false;
    bool ok = false;
    double v = s.toDouble(&ok);
    if (ok) { out = v; return true; }
    return false;
}

// SQL LIKE 匹配:`%` 任意长度,`_` 任意单字符,其余字符按字面匹配。
// 大小写敏感,与 compareValues 字符串路径一致。
bool matchesLike(const QString& s, const QString& pattern) {
    QString rx;
    rx.reserve(pattern.size() * 2 + 2);
    rx += '^';
    for (QChar c : pattern) {
        if (c == QLatin1Char('%'))      rx += QStringLiteral(".*");
        else if (c == QLatin1Char('_')) rx += QLatin1Char('.');
        else rx += QRegularExpression::escape(QString(c));
    }
    rx += '$';
    QRegularExpression re(rx, QRegularExpression::DotMatchesEverythingOption);
    return re.match(s).hasMatch();
}

bool compareValues(const QString& a, const QString& b, const QString& op) {
    double na, nb;
    bool aNum = tryToDouble(a, na);
    bool bNum = tryToDouble(b, nb);

    if (aNum && bNum) {
        if (op == "=")  return na == nb;
        if (op == "!=") return na != nb;
        if (op == "<")  return na <  nb;
        if (op == "<=") return na <= nb;
        if (op == ">")  return na >  nb;
        if (op == ">=") return na >= nb;
    } else {
        int cmp = QString::compare(a, b, Qt::CaseSensitive);
        if (op == "=")  return cmp == 0;
        if (op == "!=") return cmp != 0;
        if (op == "<")  return cmp <  0;
        if (op == "<=") return cmp <= 0;
        if (op == ">")  return cmp >  0;
        if (op == ">=") return cmp >= 0;
    }
    throw SqlError(QString("不支持的比较运算符: %1").arg(op));
}

// ----- 解析作用域 -----
struct ScopeEntry {
    QString             name;   // 别名或表名(用于限定列引用)
    const Table*        table;
    const QStringList*  row;
};

struct Scope {
    std::vector<ScopeEntry> entries;

    // 解析未限定/限定列名 → 该列在当前 row 中的字符串值
    QString resolveColumn(const ColumnRef& ref) const {
        if (!ref.table.isEmpty()) {
            for (const auto& e : entries) {
                if (e.name.compare(ref.table, Qt::CaseInsensitive) == 0) {
                    int idx = e.table->columnIndex(ref.column);
                    if (idx < 0) {
                        throw SqlError(
                            QString("表 \"%1\" 中不存在列 \"%2\"")
                                .arg(e.table->name, ref.column));
                    }
                    return (*e.row).value(idx);
                }
            }
            throw SqlError(QString("未知的表/别名 \"%1\"").arg(ref.table));
        }
        // 未限定:在所有可见表中查找
        const ScopeEntry* found = nullptr;
        int foundIdx = -1;
        for (const auto& e : entries) {
            int idx = e.table->columnIndex(ref.column);
            if (idx >= 0) {
                if (found) {
                    throw SqlError(
                        QString("列名 \"%1\" 在多个表中存在,请用 \"表名.列名\" 限定")
                            .arg(ref.column));
                }
                found = &e;
                foundIdx = idx;
            }
        }
        if (!found) {
            throw SqlError(QString("未知的列 \"%1\"").arg(ref.column));
        }
        return (*found->row).value(foundIdx);
    }
};

QString evalValue(Expr* e, const Scope& scope);

bool evalPredicate(Expr* e, const Scope& scope) {
    auto* bin = dynamic_cast<BinaryOp*>(e);
    if (!bin) {
        throw SqlError("WHERE/ON 条件必须是布尔表达式");
    }
    const QString& op = bin->op;
    if (op == "AND") return evalPredicate(bin->lhs.get(), scope) &&
                            evalPredicate(bin->rhs.get(), scope);
    if (op == "OR")  return evalPredicate(bin->lhs.get(), scope) ||
                            evalPredicate(bin->rhs.get(), scope);

    QString a = evalValue(bin->lhs.get(), scope);
    QString b = evalValue(bin->rhs.get(), scope);
    if (op == "LIKE") return matchesLike(a, b);
    return compareValues(a, b, op);
}

QString evalValue(Expr* e, const Scope& scope) {
    if (auto* lit = dynamic_cast<Literal*>(e)) {
        return lit->value;
    }
    if (auto* ref = dynamic_cast<ColumnRef*>(e)) {
        return scope.resolveColumn(*ref);
    }
    // BinaryOp 作为 value:返回 "1"/"0"
    if (auto* bin = dynamic_cast<BinaryOp*>(e)) {
        return evalPredicate(bin, scope) ? "1" : "0";
    }
    throw SqlError("非法的表达式节点");
}

} // namespace

// ============================================================

Executor::Executor(QString dataDir) : dataDir_(std::move(dataDir)) {}

QString Executor::pathFor(const QString& tableName) const {
    return QDir(dataDir_).filePath(tableName + ".csv");
}

Table& Executor::loadTable(const QString& name) {
    QString key = name.toLower();
    auto it = cache_.find(key);
    if (it != cache_.end()) return it.value();

    QString path = pathFor(name);
    if (!QFileInfo::exists(path)) {
        throw SqlError(QString("找不到表 \"%1\" (文件不存在: %2)").arg(name, path));
    }
    Table t = CsvIO::load(path, name);
    cache_.insert(key, t);
    return cache_[key];
}

void Executor::persist(const Table& t) {
    CsvIO::save(pathFor(t.name), t);
    cache_[t.name.toLower()] = t; // 同步缓存
}

Executor::Result Executor::execute(Statement* stmt) {
    if (auto* s = dynamic_cast<SelectStmt*>(stmt)) return executeSelect(s);
    if (auto* s = dynamic_cast<InsertStmt*>(stmt)) return executeInsert(s);
    if (auto* s = dynamic_cast<UpdateStmt*>(stmt)) return executeUpdate(s);
    if (auto* s = dynamic_cast<DeleteStmt*>(stmt)) return executeDelete(s);
    throw SqlError("未知的语句类型");
}

// ---------------- SELECT ----------------

Executor::Result Executor::executeSelect(SelectStmt* stmt) {
    // ---- 1) 装载所有相关表(FROM + 全部 JOIN) ----
    // 注:不能用变量名 `slots` —— 那是 Qt MOC 宏,会被展开成空 token。
    struct TableSlot {
        QString      name;   // 别名优先,否则用表名
        const Table* table;
    };
    std::vector<TableSlot> srcTables;

    Table& fromTable = loadTable(stmt->fromTable);
    srcTables.push_back({
        stmt->fromAlias.isEmpty() ? fromTable.name : stmt->fromAlias,
        &fromTable
    });
    for (const auto& jc : stmt->joins) {
        Table& jt = loadTable(jc.table);
        srcTables.push_back({
            jc.alias.isEmpty() ? jt.name : jc.alias,
            &jt
        });
    }
    const bool multiTable = srcTables.size() > 1;

    // ---- 2) 解析投影 ----
    struct ColSrc {
        QString header;
        int     slot;   // -1 表示 COUNT(*) 占位
        int     idx;
    };
    std::vector<ColSrc> outputCols;
    bool hasCountStar = false;
    bool hasRegular   = false;

    auto resolveColumnRef = [&](const QString& tableRef, const QString& column,
                                int& outSlot, int& outIdx) {
        outSlot = -1;
        outIdx  = -1;
        if (!tableRef.isEmpty()) {
            for (size_t s = 0; s < srcTables.size(); ++s) {
                if (srcTables[s].name.compare(tableRef, Qt::CaseInsensitive) == 0) {
                    outSlot = static_cast<int>(s);
                    outIdx  = srcTables[s].table->columnIndex(column);
                    return;
                }
            }
            return;
        }
        int matches = 0;
        for (size_t s = 0; s < srcTables.size(); ++s) {
            int ci = srcTables[s].table->columnIndex(column);
            if (ci >= 0) {
                ++matches;
                outSlot = static_cast<int>(s);
                outIdx  = ci;
            }
        }
        if (matches > 1) {
            throw SqlError(QString("列名 \"%1\" 在多个表中存在,请用 \"表名.列名\" 限定")
                              .arg(column));
        }
    };

    for (const auto& p : stmt->projections) {
        if (p.isCountStar) {
            hasCountStar = true;
            outputCols.push_back({QStringLiteral("COUNT(*)"), -1, -1});
            continue;
        }
        hasRegular = true;
        if (p.star) {
            for (size_t s = 0; s < srcTables.size(); ++s) {
                const TableSlot& slot = srcTables[s];
                for (int i = 0; i < slot.table->columns.size(); ++i) {
                    QString h = multiTable
                        ? (slot.name + "." + slot.table->columns[i])
                        : slot.table->columns[i];
                    outputCols.push_back({h, static_cast<int>(s), i});
                }
            }
            continue;
        }
        int slotIdx = -1, idx = -1;
        resolveColumnRef(p.table, p.column, slotIdx, idx);
        if (slotIdx < 0) {
            throw SqlError(p.table.isEmpty()
                ? QString("未知的列 \"%1\"").arg(p.column)
                : QString("未知的表/别名 \"%1\"").arg(p.table));
        }
        if (idx < 0) {
            throw SqlError(QString("表 \"%1\" 中不存在列 \"%2\"")
                              .arg(srcTables[slotIdx].name, p.column));
        }
        QString header = p.table.isEmpty()
            ? p.column
            : (p.table + "." + p.column);
        outputCols.push_back({header, slotIdx, idx});
    }

    if (hasCountStar && hasRegular) {
        throw SqlError(QStringLiteral(
            "COUNT(*) 不能与普通列混用(本引擎暂不支持 GROUP BY)"));
    }

    // ---- 3) 迭代构建 N 表笛卡尔积 + 逐次应用每个 JOIN 的 ON 过滤 ----
    using Combo = std::vector<const QStringList*>;
    std::vector<Combo> combos;
    combos.reserve(srcTables[0].table->rows.size());
    for (const auto& r : srcTables[0].table->rows) {
        Combo c;
        c.push_back(&r);
        combos.push_back(std::move(c));
    }

    auto buildScope = [&](const Combo& combo, int upto /* exclusive */) {
        Scope sc;
        sc.entries.reserve(static_cast<int>(upto));
        for (int k = 0; k < upto; ++k) {
            sc.entries.push_back({srcTables[k].name, srcTables[k].table, combo[k]});
        }
        return sc;
    };

    for (size_t j = 0; j < stmt->joins.size(); ++j) {
        const Table& rt = *srcTables[j + 1].table;
        const auto& jc = stmt->joins[j];
        std::vector<Combo> next;
        for (const auto& combo : combos) {
            for (const auto& rrow : rt.rows) {
                Combo candidate = combo;
                candidate.push_back(&rrow);
                Scope sc = buildScope(candidate, static_cast<int>(j + 2));
                if (jc.on && !evalPredicate(jc.on.get(), sc)) continue;
                next.push_back(std::move(candidate));
            }
        }
        combos = std::move(next);
    }

    // ---- 4) WHERE 过滤 ----
    std::vector<Combo> filtered;
    filtered.reserve(combos.size());
    for (auto& combo : combos) {
        if (stmt->where) {
            Scope sc = buildScope(combo, static_cast<int>(srcTables.size()));
            if (!evalPredicate(stmt->where.get(), sc)) continue;
        }
        filtered.push_back(std::move(combo));
    }

    // ---- 5) 聚合 COUNT(*):直接构造单行单列结果并返回 ----
    Table result;
    result.name = "result";
    if (hasCountStar) {
        result.columns << QStringLiteral("COUNT(*)");
        QStringList row;
        row << QString::number(static_cast<int>(filtered.size()));
        result.rows << row;
        return {QStringLiteral("SELECT"), std::move(result), 1};
    }

    // ---- 6) ORDER BY 排序(在投影前,使 ORDER BY 可引用未投影的列) ----
    if (!stmt->orderBy.empty()) {
        struct OrderKey { int slot; int idx; bool descending; };
        std::vector<OrderKey> keys;
        keys.reserve(stmt->orderBy.size());
        for (const auto& ob : stmt->orderBy) {
            int slotIdx = -1, idx = -1;
            resolveColumnRef(ob.table, ob.column, slotIdx, idx);
            if (slotIdx < 0) {
                throw SqlError(ob.table.isEmpty()
                    ? QString("ORDER BY 中未知的列 \"%1\"").arg(ob.column)
                    : QString("ORDER BY 中未知的表/别名 \"%1\"").arg(ob.table));
            }
            if (idx < 0) {
                throw SqlError(QString("ORDER BY:表 \"%1\" 中不存在列 \"%2\"")
                                  .arg(srcTables[slotIdx].name, ob.column));
            }
            keys.push_back({slotIdx, idx, ob.descending});
        }
        std::stable_sort(filtered.begin(), filtered.end(),
            [&](const Combo& a, const Combo& b) {
                for (const auto& k : keys) {
                    const QString& va = a[k.slot]->value(k.idx);
                    const QString& vb = b[k.slot]->value(k.idx);
                    double na = 0.0, nb = 0.0;
                    bool aNum = tryToDouble(va, na);
                    bool bNum = tryToDouble(vb, nb);
                    bool less, equal;
                    if (aNum && bNum) {
                        equal = (na == nb);
                        less  = (na <  nb);
                    } else {
                        int cmp = QString::compare(va, vb, Qt::CaseSensitive);
                        equal = (cmp == 0);
                        less  = (cmp <  0);
                    }
                    if (equal) continue;
                    return k.descending ? !less : less;
                }
                return false;
            });
    }

    // ---- 7) 投影 ----
    for (const auto& c : outputCols) result.columns << c.header;
    for (const auto& combo : filtered) {
        QStringList row;
        row.reserve(static_cast<int>(outputCols.size()));
        for (const auto& c : outputCols) {
            row << combo[c.slot]->value(c.idx);
        }
        result.rows << row;
    }

    const int rowCount = result.rows.size();
    return {QStringLiteral("SELECT"), std::move(result), rowCount};
}

// ---------------- INSERT ----------------

Executor::Result Executor::executeInsert(InsertStmt* stmt) {
    Table& t = loadTable(stmt->table);

    QStringList targetCols = stmt->columns.isEmpty() ? t.columns : stmt->columns;
    if (static_cast<int>(stmt->values.size()) != targetCols.size()) {
        throw SqlError(QString("列数 (%1) 与值数 (%2) 不匹配")
                          .arg(targetCols.size()).arg(stmt->values.size()));
    }
    // 校验列名是否都存在
    for (const auto& c : targetCols) {
        if (!t.hasColumn(c)) {
            throw SqlError(QString("表 \"%1\" 中不存在列 \"%2\"").arg(t.name, c));
        }
    }

    Scope emptyScope;
    QStringList newRow;
    newRow.reserve(t.columns.size());
    for (int i = 0; i < t.columns.size(); ++i) newRow << QString();
    for (int i = 0; i < targetCols.size(); ++i) {
        int idx = t.columnIndex(targetCols[i]);
        QString v = evalValue(stmt->values[i].get(), emptyScope);
        newRow[idx] = v;
    }

    t.rows << newRow;
    persist(t);
    return {QStringLiteral("INSERT"), {}, 1};
}

// ---------------- UPDATE ----------------

Executor::Result Executor::executeUpdate(UpdateStmt* stmt) {
    Table& t = loadTable(stmt->table);

    for (const auto& a : stmt->assigns) {
        if (!t.hasColumn(a.column)) {
            throw SqlError(QString("表 \"%1\" 中不存在列 \"%2\"").arg(t.name, a.column));
        }
    }

    int affected = 0;
    for (auto& row : t.rows) {
        Scope scope;
        scope.entries.push_back({t.name, &t, &row});
        if (stmt->where && !evalPredicate(stmt->where.get(), scope)) continue;

        // 先计算所有新值,再写入(避免赋值过程相互影响)
        QVector<QPair<int, QString>> updates;
        updates.reserve(static_cast<int>(stmt->assigns.size()));
        for (const auto& a : stmt->assigns) {
            int idx = t.columnIndex(a.column);
            QString v = evalValue(a.value.get(), scope);
            updates << qMakePair(idx, v);
        }
        for (const auto& u : updates) row[u.first] = u.second;
        ++affected;
    }

    if (affected > 0) persist(t);
    return {QStringLiteral("UPDATE"), {}, affected};
}

// ---------------- DELETE ----------------

Executor::Result Executor::executeDelete(DeleteStmt* stmt) {
    Table& t = loadTable(stmt->table);
    int before = t.rows.size();

    if (stmt->where) {
        QVector<QStringList> kept;
        kept.reserve(t.rows.size());
        for (const auto& row : t.rows) {
            Scope scope;
            scope.entries.push_back({t.name, &t, &row});
            if (!evalPredicate(stmt->where.get(), scope)) {
                kept << row;
            }
        }
        t.rows = std::move(kept);
    } else {
        t.rows.clear();
    }

    int affected = before - t.rows.size();
    if (affected > 0) persist(t);
    return {QStringLiteral("DELETE"), {}, affected};
}
