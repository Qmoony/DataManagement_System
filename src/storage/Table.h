#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct Table {
    QString name;
    QStringList columns;
    QVector<QStringList> rows;

    int columnIndex(const QString& col) const;
    bool hasColumn(const QString& col) const;
};
