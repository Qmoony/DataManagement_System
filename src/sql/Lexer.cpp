#include "Lexer.h"
#include "SqlError.h"

#include <QHash>

namespace {

const QHash<QString, TokenType>& keywordMap() {
    static const QHash<QString, TokenType> kw = {
        {"SELECT", TokenType::Select},
        {"FROM",   TokenType::From},
        {"WHERE",  TokenType::Where},
        {"AND",    TokenType::And},
        {"OR",     TokenType::Or},
        {"JOIN",   TokenType::Join},
        {"ON",     TokenType::On},
        {"AS",     TokenType::As},
        {"INSERT", TokenType::Insert},
        {"INTO",   TokenType::Into},
        {"VALUES", TokenType::Values},
        {"UPDATE", TokenType::Update},
        {"SET",    TokenType::Set},
        {"DELETE", TokenType::Delete},
        {"ORDER",  TokenType::Order},
        {"BY",     TokenType::By},
        {"ASC",    TokenType::Asc},
        {"DESC",   TokenType::Desc},
        {"LIKE",   TokenType::Like},
        {"COUNT",  TokenType::Count},
    };
    return kw;
}

bool isIdentStart(QChar c) {
    return c.isLetter() || c == '_';
}
bool isIdentCont(QChar c) {
    return c.isLetterOrNumber() || c == '_';
}

} // namespace

Lexer::Lexer(QString source) : src_(std::move(source)) {}

QVector<Token> Lexer::tokenize() {
    QVector<Token> out;
    while (true) {
        Token t = nextToken();
        out << t;
        if (t.type == TokenType::End) break;
    }
    return out;
}

void Lexer::skipWhitespace() {
    while (pos_ < src_.size()) {
        QChar c = src_[pos_];
        if (c == '\n' || c == '\r') {
            ++pos_; ++col_;
        } else if (c.isSpace()) {
            ++pos_; ++col_;
        } else {
            break;
        }
    }
}

Token Lexer::nextToken() {
    skipWhitespace();
    if (pos_ >= src_.size()) {
        return {TokenType::End, QString(), col_};
    }

    int startCol = col_;
    QChar c = src_[pos_];

    if (c.isDigit()) return readNumber();
    if (isIdentStart(c)) return readIdentifierOrKeyword();
    if (c == '\'' || c == '"') return readString(c);

    // 单字符或双字符符号
    auto consume = [&](TokenType t, int len = 1) {
        QString txt = src_.mid(pos_, len);
        Token tok{t, txt, startCol};
        pos_ += len; col_ += len;
        return tok;
    };

    switch (c.unicode()) {
        case ',': return consume(TokenType::Comma);
        case '.': return consume(TokenType::Dot);
        case '(': return consume(TokenType::LParen);
        case ')': return consume(TokenType::RParen);
        case ';': return consume(TokenType::Semicolon);
        case '*': return consume(TokenType::Star);
        case '=': return consume(TokenType::Eq);
        case '<':
            if (pos_ + 1 < src_.size() && src_[pos_ + 1] == '=')
                return consume(TokenType::Le, 2);
            return consume(TokenType::Lt);
        case '>':
            if (pos_ + 1 < src_.size() && src_[pos_ + 1] == '=')
                return consume(TokenType::Ge, 2);
            return consume(TokenType::Gt);
        case '!':
            if (pos_ + 1 < src_.size() && src_[pos_ + 1] == '=')
                return consume(TokenType::Neq, 2);
            throw SqlError(QString("非法字符: !"), startCol);
        default: break;
    }

    throw SqlError(QString("非法字符: %1").arg(c), startCol);
}

Token Lexer::readNumber() {
    int startCol = col_;
    int startPos = pos_;
    bool seenDot = false;
    while (pos_ < src_.size()) {
        QChar c = src_[pos_];
        if (c.isDigit()) {
            ++pos_; ++col_;
        } else if (c == '.' && !seenDot) {
            seenDot = true;
            ++pos_; ++col_;
        } else {
            break;
        }
    }
    return {TokenType::Number, src_.mid(startPos, pos_ - startPos), startCol};
}

Token Lexer::readIdentifierOrKeyword() {
    int startCol = col_;
    int startPos = pos_;
    while (pos_ < src_.size() && isIdentCont(src_[pos_])) {
        ++pos_; ++col_;
    }
    QString text = src_.mid(startPos, pos_ - startPos);
    QString upper = text.toUpper();
    auto it = keywordMap().find(upper);
    if (it != keywordMap().end()) {
        return {it.value(), upper, startCol};
    }
    return {TokenType::Identifier, text, startCol};
}

Token Lexer::readString(QChar quote) {
    int startCol = col_;
    ++pos_; ++col_; // 跳过开引号
    QString value;
    while (pos_ < src_.size()) {
        QChar c = src_[pos_];
        if (c == quote) {
            // 双引号转义
            if (pos_ + 1 < src_.size() && src_[pos_ + 1] == quote) {
                value += quote;
                pos_ += 2; col_ += 2;
                continue;
            }
            ++pos_; ++col_;
            return {TokenType::String, value, startCol};
        }
        if (c == '\n') {
            throw SqlError("字符串字面量中出现换行,缺少右引号", startCol);
        }
        value += c;
        ++pos_; ++col_;
    }
    throw SqlError("字符串字面量未闭合", startCol);
}
