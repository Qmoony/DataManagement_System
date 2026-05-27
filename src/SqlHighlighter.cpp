#include "SqlHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextCursor>
#include <QTextDocument>

SqlHighlighter::SqlHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    // ---- 1. 注释 (最后匹配,优先级最高,先加 → 后被字符串覆盖不了) ----
    // 放最后再加,这里先占位,实际顺序见下方

    // ---- 2. 关键字:暖珊瑚 + 粗体 ----
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
        rules_.push_back({
            QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw),
                               QRegularExpression::CaseInsensitiveOption),
            keywordFmt
        });
    }

    // ---- 3. 数字:柔和绿 ----
    QTextCharFormat numberFmt;
    numberFmt.setForeground(QColor(QStringLiteral("#98C379")));
    rules_.push_back({
        QRegularExpression(QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?\\b")),
        numberFmt
    });

    // ---- 4. 运算符:淡紫 ----
    QTextCharFormat opFmt;
    opFmt.setForeground(QColor(QStringLiteral("#BB9AF7")));
    rules_.push_back({
        QRegularExpression(QStringLiteral("[=!<>]+")),
        opFmt
    });

    // ---- 5. 表名:柔金 ----
    QTextCharFormat tableFmt;
    tableFmt.setForeground(QColor(QStringLiteral("#D4A574")));
    rules_.push_back({
        QRegularExpression(
            QStringLiteral("\\b(?:studentTable|classTable|courseTable|scoreTable)\\b"),
            QRegularExpression::CaseInsensitiveOption),
        tableFmt
    });

    // ---- 6. 列名:蓝色 ----
    QTextCharFormat colFmt;
    colFmt.setForeground(QColor(QStringLiteral("#7DCFFF")));
    rules_.push_back({
        QRegularExpression(
            QStringLiteral("\\b(?:id|name|sex|class_id|stu_id|course_id|score)\\b"),
            QRegularExpression::CaseInsensitiveOption),
        colFmt
    });

    // ---- 7. 字符串:覆盖列名/表名 ----
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor(QStringLiteral("#D4A574")));
    rules_.push_back({QRegularExpression(QStringLiteral("'(?:[^']|'')*'")), stringFmt});
    rules_.push_back({QRegularExpression(QStringLiteral("\"(?:[^\"]|\"\")*\"")), stringFmt});

    // ---- 8. 注释:灰斜体,最后加 → 覆盖所有 ----
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor(QStringLiteral("#565F89")));
    commentFmt.setFontItalic(true);
    rules_.push_back({
        QRegularExpression(QStringLiteral("--[^\\n]*")),
        commentFmt
    });
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

void SqlHighlighter::markError(int col) {
    if (col <= 0 || !document()) return;
    const QString text = document()->toPlainText();
    const int pos = col - 1; // 转为 0-based
    if (pos >= text.length()) return;

    QTextCursor cursor(document());
    cursor.setPosition(pos);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    if (cursor.anchor() == cursor.position())
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    fmt.setUnderlineColor(QColor(QStringLiteral("#FF5555")));
    cursor.mergeCharFormat(fmt);
}
