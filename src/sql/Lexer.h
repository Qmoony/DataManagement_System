#pragma once

#include "Token.h"

#include <QString>
#include <QVector>

class Lexer {
public:
    explicit Lexer(QString source);
    QVector<Token> tokenize();

private:
    Token nextToken();
    void  skipWhitespace();
    Token readNumber();
    Token readIdentifierOrKeyword();
    Token readString(QChar quote);

    const QString src_;
    int           pos_  = 0;
    int           col_  = 1; // 1-based,用于错误定位
};
