#pragma once

#include "Ast.h"
#include "Token.h"

#include <QVector>

class Parser {
public:
    explicit Parser(QVector<Token> tokens);
    StmtPtr parse();

private:
    // 语句产生式
    StmtPtr parseSelect();
    StmtPtr parseInsert();
    StmtPtr parseUpdate();
    StmtPtr parseDelete();

    // 表达式产生式
    ExprPtr parseExpr();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseCmp();
    ExprPtr parsePrimary();

    // 工具
    const Token& peek(int offset = 0) const;
    const Token& consume();
    bool match(TokenType t);
    const Token& expect(TokenType t, const QString& what);
    QString opSymbol(TokenType t) const;
    bool isCmpOp(TokenType t) const;

    // 投影 / 限定列名
    void parseProjections(std::vector<ProjItem>& out);
    void parseQualifiedName(QString& table, QString& column);

    QVector<Token> tokens_;
    int            cur_ = 0;
};
