#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class SqlHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SqlHighlighter(QTextDocument* parent = nullptr);

    void markError(int col); // 在第 col 列(1-based)画红色波浪下划线

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<Rule> rules_;
};
