#include "Parser.h"
#include "SqlError.h"

Parser::Parser(QVector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    int idx = cur_ + offset;
    if (idx >= tokens_.size()) return tokens_.last();
    return tokens_[idx];
}

const Token& Parser::consume() {
    const Token& t = tokens_[cur_];
    if (cur_ + 1 < tokens_.size()) ++cur_;
    return t;
}

bool Parser::match(TokenType t) {
    if (peek().type == t) { consume(); return true; }
    return false;
}

const Token& Parser::expect(TokenType t, const QString& what) {
    if (peek().type != t) {
        const Token& got = peek();
        throw SqlError(
            QString("期望 %1,但遇到 \"%2\"").arg(what, got.text.isEmpty() ? "<结束>" : got.text),
            got.pos);
    }
    return consume();
}

QString Parser::opSymbol(TokenType t) const {
    switch (t) {
        case TokenType::Eq:   return "=";
        case TokenType::Neq:  return "!=";
        case TokenType::Lt:   return "<";
        case TokenType::Le:   return "<=";
        case TokenType::Gt:   return ">";
        case TokenType::Ge:   return ">=";
        case TokenType::Like: return "LIKE";
        default: return {};
    }
}

bool Parser::isCmpOp(TokenType t) const {
    return t == TokenType::Eq || t == TokenType::Neq ||
           t == TokenType::Lt || t == TokenType::Le ||
           t == TokenType::Gt || t == TokenType::Ge ||
           t == TokenType::Like;
}

StmtPtr Parser::parse() {
    if (tokens_.isEmpty() || peek().type == TokenType::End) {
        throw SqlError("语句为空");
    }
    StmtPtr stmt;
    switch (peek().type) {
        case TokenType::Select: stmt = parseSelect(); break;
        case TokenType::Insert: stmt = parseInsert(); break;
        case TokenType::Update: stmt = parseUpdate(); break;
        case TokenType::Delete: stmt = parseDelete(); break;
        default:
            throw SqlError(
                QString("语句必须以 SELECT/INSERT/UPDATE/DELETE 开头,实际为 \"%1\"")
                    .arg(peek().text),
                peek().pos);
    }
    // 末尾可选分号
    match(TokenType::Semicolon);
    if (peek().type != TokenType::End) {
        throw SqlError(QString("语句结束后出现多余记号: \"%1\"").arg(peek().text),
                       peek().pos);
    }
    return stmt;
}

void Parser::parseQualifiedName(QString& table, QString& column) {
    const Token& a = expect(TokenType::Identifier, "标识符");
    if (peek().type == TokenType::Dot) {
        consume();
        const Token& b = expect(TokenType::Identifier, "点号后的列名");
        table = a.text;
        column = b.text;
    } else {
        table.clear();
        column = a.text;
    }
}

void Parser::parseProjections(std::vector<ProjItem>& out) {
    auto parseOne = [&]() {
        if (peek().type == TokenType::Star) {
            consume();
            ProjItem p; p.star = true; p.column = "*";
            out.push_back(std::move(p));
        } else if (peek().type == TokenType::Count) {
            consume();
            expect(TokenType::LParen, "COUNT 后的左括号");
            if (peek().type != TokenType::Star) {
                throw SqlError(QStringLiteral("仅支持 COUNT(*),不支持 COUNT(<列>)"),
                               peek().pos);
            }
            consume(); // *
            expect(TokenType::RParen, "COUNT(*) 的右括号");
            ProjItem p;
            p.isCountStar = true;
            p.column = "COUNT(*)";
            out.push_back(std::move(p));
        } else {
            ProjItem p;
            parseQualifiedName(p.table, p.column);
            out.push_back(std::move(p));
        }
    };
    parseOne();
    while (match(TokenType::Comma)) parseOne();
}

StmtPtr Parser::parseSelect() {
    expect(TokenType::Select, "SELECT");
    auto stmt = std::make_unique<SelectStmt>();
    parseProjections(stmt->projections);

    expect(TokenType::From, "FROM");
    const Token& tbl = expect(TokenType::Identifier, "表名");
    stmt->fromTable = tbl.text;

    // 别名:`from t alias` 或 `from t as alias`
    if (peek().type == TokenType::As) {
        consume();
        stmt->fromAlias = expect(TokenType::Identifier, "AS 后的别名").text;
    } else if (peek().type == TokenType::Identifier) {
        stmt->fromAlias = consume().text;
    }

    // 可选 JOIN 链(零个或多个)
    while (peek().type == TokenType::Join) {
        consume();
        JoinClause jc;
        jc.table = expect(TokenType::Identifier, "JOIN 后的表名").text;
        if (peek().type == TokenType::As) {
            consume();
            jc.alias = expect(TokenType::Identifier, "AS 后的别名").text;
        } else if (peek().type == TokenType::Identifier) {
            jc.alias = consume().text;
        }
        expect(TokenType::On, "ON");
        jc.on = parseExpr();
        stmt->joins.push_back(std::move(jc));
    }

    // 可选 WHERE
    if (match(TokenType::Where)) {
        stmt->where = parseExpr();
    }

    // 可选 ORDER BY <col> [ASC|DESC] [, ...]
    if (match(TokenType::Order)) {
        expect(TokenType::By, "ORDER 后的 BY");
        auto parseOrderItem = [&]() {
            OrderByItem item;
            parseQualifiedName(item.table, item.column);
            if (match(TokenType::Desc))      item.descending = true;
            else if (match(TokenType::Asc))  item.descending = false;
            stmt->orderBy.push_back(std::move(item));
        };
        parseOrderItem();
        while (match(TokenType::Comma)) parseOrderItem();
    }

    return stmt;
}

StmtPtr Parser::parseInsert() {
    expect(TokenType::Insert, "INSERT");
    expect(TokenType::Into, "INTO");
    auto stmt = std::make_unique<InsertStmt>();
    stmt->table = expect(TokenType::Identifier, "INTO 后的表名").text;

    // 可选列名列表
    if (match(TokenType::LParen)) {
        stmt->columns << expect(TokenType::Identifier, "列名").text;
        while (match(TokenType::Comma)) {
            stmt->columns << expect(TokenType::Identifier, "列名").text;
        }
        expect(TokenType::RParen, "右括号");
    }

    expect(TokenType::Values, "VALUES");
    expect(TokenType::LParen, "左括号");
    stmt->values.push_back(parseExpr());
    while (match(TokenType::Comma)) {
        stmt->values.push_back(parseExpr());
    }
    expect(TokenType::RParen, "右括号");

    return stmt;
}

StmtPtr Parser::parseUpdate() {
    expect(TokenType::Update, "UPDATE");
    auto stmt = std::make_unique<UpdateStmt>();
    stmt->table = expect(TokenType::Identifier, "表名").text;
    expect(TokenType::Set, "SET");

    auto parseOneAssign = [&]() {
        Assignment a;
        a.column = expect(TokenType::Identifier, "列名").text;
        expect(TokenType::Eq, "=");
        a.value = parseExpr();
        stmt->assigns.push_back(std::move(a));
    };
    parseOneAssign();
    while (match(TokenType::Comma)) parseOneAssign();

    if (match(TokenType::Where)) {
        stmt->where = parseExpr();
    }
    return stmt;
}

StmtPtr Parser::parseDelete() {
    expect(TokenType::Delete, "DELETE");
    expect(TokenType::From, "FROM");
    auto stmt = std::make_unique<DeleteStmt>();
    stmt->table = expect(TokenType::Identifier, "表名").text;
    if (match(TokenType::Where)) {
        stmt->where = parseExpr();
    }
    return stmt;
}

// ---------------- 表达式 ----------------

ExprPtr Parser::parseExpr() {
    return parseOr();
}

ExprPtr Parser::parseOr() {
    ExprPtr lhs = parseAnd();
    while (peek().type == TokenType::Or) {
        consume();
        auto node = std::make_unique<BinaryOp>();
        node->op = "OR";
        node->lhs = std::move(lhs);
        node->rhs = parseAnd();
        lhs = std::move(node);
    }
    return lhs;
}

ExprPtr Parser::parseAnd() {
    ExprPtr lhs = parseCmp();
    while (peek().type == TokenType::And) {
        consume();
        auto node = std::make_unique<BinaryOp>();
        node->op = "AND";
        node->lhs = std::move(lhs);
        node->rhs = parseCmp();
        lhs = std::move(node);
    }
    return lhs;
}

ExprPtr Parser::parseCmp() {
    ExprPtr lhs = parsePrimary();
    if (isCmpOp(peek().type)) {
        QString op = opSymbol(peek().type);
        consume();
        ExprPtr rhs = parsePrimary();
        auto node = std::make_unique<BinaryOp>();
        node->op = op;
        node->lhs = std::move(lhs);
        node->rhs = std::move(rhs);
        return node;
    }
    return lhs;
}

ExprPtr Parser::parsePrimary() {
    const Token& t = peek();
    if (t.type == TokenType::LParen) {
        consume();
        ExprPtr inner = parseExpr();
        expect(TokenType::RParen, "右括号");
        return inner;
    }
    if (t.type == TokenType::Number) {
        consume();
        auto lit = std::make_unique<Literal>();
        lit->value = t.text;
        lit->quoted = false;
        return lit;
    }
    if (t.type == TokenType::String) {
        consume();
        auto lit = std::make_unique<Literal>();
        lit->value = t.text;
        lit->quoted = true;
        return lit;
    }
    if (t.type == TokenType::Identifier) {
        QString table, col;
        parseQualifiedName(table, col);
        auto ref = std::make_unique<ColumnRef>();
        ref->table = table;
        ref->column = col;
        return ref;
    }
    throw SqlError(QString("意外的记号 \"%1\"").arg(t.text), t.pos);
}
