#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

// --------- 表达式 ---------
struct Expr {
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct Literal : Expr {
    QString value;
    bool    quoted = false; // true: 原本是字符串字面量
};

struct ColumnRef : Expr {
    QString table;  // 限定前缀,空表示未限定
    QString column;
};

struct BinaryOp : Expr {
    QString op;     // = != < <= > >= AND OR
    ExprPtr lhs;
    ExprPtr rhs;
};

// --------- 投影项 ---------
struct ProjItem {
    QString table;       // 限定前缀,空表示未限定
    QString column;      // 列名 / "*"
    bool    star = false;
    bool    isCountStar = false; // COUNT(*),仅支持这一种聚合形式
};

// --------- JOIN 子句 ---------
struct JoinClause {
    QString table;
    QString alias;
    ExprPtr on;
};

// --------- ORDER BY 项 ---------
struct OrderByItem {
    QString table;       // 限定前缀
    QString column;
    bool    descending = false;
};

// --------- 赋值 ---------
struct Assignment {
    QString column;
    ExprPtr value;
};

// --------- 语句基类 ---------
struct Statement {
    virtual ~Statement() = default;
};
using StmtPtr = std::unique_ptr<Statement>;

struct SelectStmt : Statement {
    std::vector<ProjItem>     projections;
    QString                   fromTable;
    QString                   fromAlias;
    std::vector<JoinClause>   joins;       // 多表 INNER JOIN 链(可为空)
    ExprPtr                   where;
    std::vector<OrderByItem>  orderBy;     // 多键排序(可为空)
};

struct InsertStmt : Statement {
    QString               table;
    QStringList           columns; // 空表示按表原列顺序
    std::vector<ExprPtr>  values;
};

struct UpdateStmt : Statement {
    QString                 table;
    std::vector<Assignment> assigns;
    ExprPtr                 where;
};

struct DeleteStmt : Statement {
    QString table;
    ExprPtr where;
};
