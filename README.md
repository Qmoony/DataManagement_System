# 数据管理系统

> 数据结构课程设计 · 邱建勇

基于 Qt6 的桌面 GUI 应用，内置手写的迷你 SQL 引擎，对四张 CSV 表（班级 / 学生 / 课程 / 成绩）执行 SELECT / INSERT / UPDATE / DELETE 操作。

---

## 功能特性

- **SQL 编辑器**：语法高亮（关键字大小写不敏感）、错误提示（精确到列位置）
- **多表 JOIN**：支持链式 `INNER JOIN ... ON`
- **分页渲染**：SELECT 结果每页 20 行，翻页不重跑查询
- **即时落盘**：INSERT / UPDATE / DELETE 修改后立即写回 CSV

## 支持的 SQL 方言

```sql
-- 多表联查
SELECT s.name, c.name, sc.score
  FROM studentTable s
  JOIN scoreTable sc ON sc.stu_id = s.id
  JOIN courseTable c  ON c.id = sc.course_id
 WHERE sc.score >= 90
 ORDER BY sc.score DESC;

-- 插入
INSERT INTO studentTable VALUES (1001, '张三', '男', 1);

-- 更新
UPDATE scoreTable SET score = 95 WHERE stu_id = 1001 AND course_id = 2;

-- 删除
DELETE FROM scoreTable WHERE score < 60;
```

> **不支持：** `IN` / `IS NULL` / `GROUP BY` / `HAVING` / `LIMIT` / 子查询 / 算术表达式 / 除 `COUNT(*)` 外的聚合 / 除 `INNER JOIN` 外的 JOIN 类型

## 数据表结构

| 表 | 列 |
|---|---|
| `classTable`   | `id, name` |
| `studentTable` | `id, name, sex, class_id` |
| `courseTable`  | `id, name` |
| `scoreTable`   | `stu_id, course_id, score` |

## 构建与运行

**依赖：** Qt 6.x · CMake ≥ 3.20 · Qt 自带 MinGW 13.1.0
（**不要**使用系统 MinGW / MSVC，ABI 不兼容）

```powershell
# 配置
cmake -S . -B cmake-build-debug -G "MinGW Makefiles"

# 编译（post-build 自动拷贝 CSV 数据和 Qt 运行时）
cmake --build cmake-build-debug

# 运行
.\cmake-build-debug\DataManagement_System.exe
```

用 CLion 直接打开根目录即可，IDE 会自动读取 `.idea/` 配置。

## 架构

```
QPlainTextEdit (SQL 输入 + SqlHighlighter 着色)
  → Lexer           词法分析，关键字大小写不敏感
  → Parser          手写递归下降，产 AST
  → Executor        解释执行；CsvIO 懒加载并缓存到 QHash<QString,Table>
  → CsvIO           RFC4180 风格 UTF-8 读写
  → QTableWidget    分页结果展示（kPageSize=20）
```

## 项目结构

```
DataManagement_System/
├── src/
│   ├── main.cpp
│   ├── MainWindow.cpp / .h
│   ├── SqlHighlighter.cpp / .h
│   ├── sql/          # SQL 引擎（Lexer / Parser / Executor / AST）
│   └── storage/      # 数据层（Table / CsvIO）
├── data/             # CSV 数据文件（四张表）
├── resources/        # QSS 样式表 + SVG 插画
├── CMakeLists.txt
└── README.md
```
