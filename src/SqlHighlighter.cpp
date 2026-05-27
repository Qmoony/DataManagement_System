#include "SqlHighlighter.h"

#include <QColor>
#include <QFont>

SqlHighlighter::SqlHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    // 关键字:Tokyo Twilight 暖珊瑚 + 粗体
    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(QColor(QStringLiteral("#FF6B5C")));
    keywordFmt.setFontWeight(QFont::Bold);

    static const QStringList kKeywords = {
        QStringLiteral("SELECT"), QStringLiteral("FROM"), QStringLiteral("WHERE"),
        QStringLiteral("AND"),    QStringLiteral("OR"),   QStringLiteral("JOIN"),
        QStringLiteral("ON"),     QStringLiteral("AS"),
        QStringLiteral("INSERT"), QStringLiteral("INTO"), QStringLiteral("VALUES"),
        QStringLiteral("UPDATE"), QStringLiteral("SET"),  QStringLiteral("DELETE"),
        QStringLiteral("ORDER"),  QStringLiteral("BY"),   QStringLiteral("ASC"),
        QStringLiteral("DESC"),   QStringLiteral("LIKE"), QStringLiteral("COUNT")
    };
    for (const QString& kw : kKeywords) {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("\\b%1\\b").arg(kw),
            QRegularExpression::CaseInsensitiveOption);
        r.format = keywordFmt;
        rules_.push_back(r);
    }

    // 数字:柔和绿
    QTextCharFormat numberFmt;
    numberFmt.setForeground(QColor(QStringLiteral("#98C379")));
    Rule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?\\b"));
    numberRule.format  = numberFmt;
    rules_.push_back(numberRule);

    // 字符串:柔金。允许 '' / "" 转义,与 Lexer 行为一致。
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor(QStringLiteral("#D4A574")));
    rules_.push_back({QRegularExpression(QStringLiteral("'(?:[^']|'')*'")), stringFmt});
    rules_.push_back({QRegularExpression(QStringLiteral("\"(?:[^\"]|\"\")*\"")), stringFmt});
}

void SqlHighlighter::highlightBlock(const QString& text) {
    for (const Rule& r : rules_) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }
}
