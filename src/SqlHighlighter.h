#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class SqlHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SqlHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<Rule> rules_;
};
