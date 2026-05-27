#include "CsvIO.h"

#include <QFile>
#include <QTextStream>
#include <stdexcept>

namespace {

// 按 RFC 4180 风格解析一行 CSV: 引号包裹字段、双引号转义。
QStringList parseLine(const QString& line) {
    QStringList out;
    QString cur;
    bool inQuotes = false;
    int i = 0;
    while (i < line.size()) {
        QChar c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    i += 2;
                    continue;
                }
                inQuotes = false;
                ++i;
                continue;
            }
            cur += c;
            ++i;
        } else {
            if (c == ',') {
                out << cur;
                cur.clear();
                ++i;
                continue;
            }
            if (c == '"' && cur.isEmpty()) {
                inQuotes = true;
                ++i;
                continue;
            }
            cur += c;
            ++i;
        }
    }
    out << cur;
    return out;
}

QString quoteIfNeeded(const QString& cell) {
    bool needQuote = cell.contains(',') || cell.contains('"') ||
                     cell.contains('\n') || cell.contains('\r');
    if (!needQuote) return cell;
    QString escaped = cell;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}

} // namespace

namespace CsvIO {

Table load(const QString& filepath, const QString& tableName) {
    QFile f(filepath);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        throw std::runtime_error(
            QString("无法打开数据文件: %1").arg(filepath).toStdString());
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    Table t;
    t.name = tableName;

    bool gotHeader = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        // 跳过空白行
        if (line.trimmed().isEmpty()) continue;
        QStringList cells = parseLine(line);
        if (!gotHeader) {
            t.columns = cells;
            gotHeader = true;
            continue;
        }
        // 缺列时补空,溢出列裁掉
        while (cells.size() < t.columns.size()) cells << QString();
        if (cells.size() > t.columns.size()) cells = cells.mid(0, t.columns.size());
        t.rows << cells;
    }

    if (!gotHeader) {
        throw std::runtime_error(
            QString("数据文件为空或无表头: %1").arg(filepath).toStdString());
    }
    return t;
}

void save(const QString& filepath, const Table& table) {
    QFile f(filepath);
    if (!f.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        throw std::runtime_error(
            QString("无法写入数据文件: %1").arg(filepath).toStdString());
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    QStringList headerOut;
    headerOut.reserve(table.columns.size());
    for (const auto& c : table.columns) headerOut << quoteIfNeeded(c);
    out << headerOut.join(',') << '\n';

    for (const auto& row : table.rows) {
        QStringList cells;
        cells.reserve(row.size());
        for (const auto& v : row) cells << quoteIfNeeded(v);
        out << cells.join(',') << '\n';
    }
}

} // namespace CsvIO
