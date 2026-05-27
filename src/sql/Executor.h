#pragma once

#include "Ast.h"
#include "../storage/Table.h"

#include <QHash>
#include <QString>

class Executor {
public:
    struct Result {
        QString operation;     // "SELECT" / "INSERT" / "UPDATE" / "DELETE"
        Table   resultTable;   // SELECT 时填充
        int     affectedRows = 0;
    };

    explicit Executor(QString dataDir);

    Result execute(Statement* stmt);

private:
    Result executeSelect(SelectStmt* stmt);
    Result executeInsert(InsertStmt* stmt);
    Result executeUpdate(UpdateStmt* stmt);
    Result executeDelete(DeleteStmt* stmt);

    // 数据加载/持久化
    Table& loadTable(const QString& name);
    void   persist(const Table& t);
    QString pathFor(const QString& tableName) const;

    QString                 dataDir_;
    QHash<QString, Table>   cache_;
};
