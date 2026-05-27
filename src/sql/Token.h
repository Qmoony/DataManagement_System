#pragma once

#include <QString>

enum class TokenType {
    // 关键字
    Select, From, Where, And, Or, Join, On, As,
    Insert, Into, Values,
    Update, Set,
    Delete,
    Order, By, Asc, Desc,
    Like, Count,

    // 字面量与标识符
    Identifier, Number, String,

    // 符号
    Star, Comma, Dot, LParen, RParen, Semicolon,

    // 比较运算符
    Eq, Neq, Lt, Le, Gt, Ge,

    // 终结符 / 错误
    End,
    Invalid
};

struct Token {
    TokenType type    = TokenType::Invalid;
    QString   text;
    int       pos     = 0; // 1-based 列位置
};
