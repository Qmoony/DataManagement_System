# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

数据结构课程设计：Qt6 桌面 GUI，内置一个手写的迷你 SQL 引擎，对 [data/](data/) 下四张 CSV 表（classTable / studentTable / courseTable / scoreTable）执行 SELECT / INSERT / UPDATE / DELETE。学号/课号/班级以数值列建模，外键关系靠 `JOIN ... ON` 表达。

作者身份信息写在 UI 中（窗口标题、欢迎语），值为 **邱建勇**。如需修改，搜 [src/MainWindow.cpp](src/MainWindow.cpp) 里 `setWindowTitle` / `headerTitle_`。

## Toolchain（重要）

CLion + CMake + Qt6 (Widgets)。**必须使用 Qt 自带的 MinGW 13.1.0 工具链**编译——不要切到 MSYS2 GCC 或系统 MinGW，因为 Qt 二进制与之 ABI 不兼容，链接会失败。CLion 工程文件 ([.idea/](.idea/)) 已经把 CMake profile 指向 Qt bundled MinGW。

构建产物中包含 `windeployqt.exe` 的 post-build 步骤，仅 Windows。

## 构建与运行

```powershell
# 配置（CLion 会自动做，命令行手动跑时这样）
cmake -S . -B cmake-build-debug -G "MinGW Makefiles"

# 编译
cmake --build cmake-build-debug

# 运行（exe 与 data/ 同目录，CMake post-build 已经拷过去了）
.\cmake-build-debug\DataManagement_System.exe
```

没有测试套件、没有 linter；手工通过 GUI 输入 SQL 验证。

## 架构

```
[QPlainTextEdit SQL  + SqlHighlighter 着色]
   → Lexer (src/sql/Lexer.cpp)       关键字大小写不敏感，identifier 保留原样
   → Parser (src/sql/Parser.cpp)     手写递归下降，产 AST in src/sql/Ast.h
   → Executor (src/sql/Executor.cpp) 解释执行；首次访问表时 CsvIO::load 并缓存到 QHash<QString, Table>
   → CsvIO (src/storage/CsvIO.cpp)   RFC4180 风格读写，UTF-8
   → QStackedWidget(QTableWidget | 空状态插画) + 分页栏 (MainWindow::renderResult / refreshPage)
```

SELECT 结果整表缓存于 [`MainWindow::cachedResult_`](src/MainWindow.h)，UI 按 `kPageSize=20` 行/页切片渲染，翻页不重跑 SQL。INSERT/UPDATE/DELETE 不进缓存,直接弹空状态卡片。

错误一律抛 `SqlError`（带 1-based 列位置 [src/sql/SqlError.h](src/sql/SqlError.h)），在 [`MainWindow::onExecute`](src/MainWindow.cpp) 顶层 catch 后用 `QMessageBox` 弹出。

### Executor 关键不变量

- **写操作即时落盘**：INSERT/UPDATE/DELETE 修改 `cache_[name]` 后立刻 `CsvIO::save` 全量重写 CSV。任何对 `Table&` 的修改路径都要保证最终 `persist(t)`。
- **`loadTable` 返回引用**：因为依赖 cache 内的稳定引用，所以 [`Executor::cache_`](src/sql/Executor.h) 必须是 `QHash<QString, Table>`，不要换成会重哈希失效引用的容器或改成 `Table` by-value。
- **JOIN 支持多层 INNER JOIN 链**：`SelectStmt::joins` 是 `std::vector<JoinClause>`；[`executeSelect`](src/sql/Executor.cpp) 迭代地把每个 JOIN 的 ON 应用到当前 combos 上(`std::vector<const QStringList*>` per 行)，避免一次性生成全笛卡尔积。新增 JOIN 类型(LEFT / RIGHT 等)要改 Parser 的 `parseSelect` 与 Executor 的 combos 扩张分支。
- **列名解析大小写不敏感**（`Table::columnIndex` 用 `Qt::CaseInsensitive`），但**字符串比较大小写敏感**（[`compareValues`](src/sql/Executor.cpp) 用 `Qt::CaseSensitive`，`matchesLike` 同样大小写敏感）。数值可比时按数值比,否则按字符串字典序比——别引入隐式类型转换。
- **WHERE / ON 必须是布尔表达式**：顶层 expr 必须是 `BinaryOp`，否则 `evalPredicate` 抛错。
- **COUNT(\*) 单独成投影**：若 `SELECT COUNT(*), col ...` 这样混用聚合与普通列,Executor 抛错(无 GROUP BY 时语义不明)。如要支持 GROUP BY,需在 SelectStmt 引入分组键并在 Executor 第 5 步分桶聚合。

### SQL 方言要点

支持的关键字（大小写不敏感）：`SELECT FROM WHERE AND OR JOIN ON AS INSERT INTO VALUES UPDATE SET DELETE ORDER BY ASC DESC LIKE COUNT`。比较算子：`= != < <= > >= LIKE`。LIKE 用 SQL 通配符 `%`(任意长度) 和 `_`(单字符),大小写敏感。**不支持** `IN / IS NULL / GROUP BY / HAVING / LIMIT / 子查询 / 算术表达式 / 除 COUNT(\*) 之外的聚合 / 除 INNER JOIN 之外的 JOIN 类型`。字符串字面量用单引号或双引号，重复引号转义。

### CSV / 数据目录

[`MainWindow::dataDir`](src/MainWindow.cpp) 先找 `<exe-dir>/data`，再退一级 `<exe-dir>/../data`，都没有就让后续 IO 自然报"找不到表"。开发期改 CSV 直接编辑 [data/](data/)，下次构建 CMake 会拷到 build 目录；运行中改 CSV 不会被 reload，需要重启或重新执行触发 cache miss。

### CSV Schema 速查(列名 snake_case,列名解析大小写不敏感)

| 表 | 列 | 关系 |
|---|---|---|
| classTable    | `id, name`                       | id 班级号 |
| studentTable  | `id, name, sex, class_id`        | id 学号;class_id → classTable.id |
| courseTable   | `id, name`                       | id 课号 |
| scoreTable    | `stu_id, course_id, score`       | (stu_id, course_id) 复合主键;stu_id → studentTable.id,course_id → courseTable.id |

写 SQL 时务必用真实列名(`class_id` / `stu_id` / `course_id`,**不是** `classId` / `studentId` / `courseId`),否则 Executor 抛"未知的列"。

## 资源 / 样式

[resources/style.qss](resources/style.qss) 编译进 qrc (`:/style.qss`)，在 [main.cpp](src/main.cpp) 启动时整段 setStyleSheet。改 UI 主题先改 QSS，不要在 C++ 里 hardcode 颜色。

## Skills 调用规则

任何操作前先对照下表——匹配即调用，不要跳过：

| 触发场景 | 必须调用的 Skill |
|---|---|
| 新增功能 / 改 UI 行为 / 修改业务逻辑 | `superpowers:brainstorming` |
| 遇到 Bug / 崩溃 / 意外输出 | `superpowers:systematic-debugging` |
| 编写或重构 C++ / QSS 代码 | `andrej-karpathy-skills:karpathy-guidelines` |
| 改动 Qt UI 布局或 QSS 样式 | `frontend-design:frontend-design` |
| 实现步骤超过 2 步的任务 | `superpowers:writing-plans` → `superpowers:executing-plans` |
| 声称修复完成 / 准备提交前 | `superpowers:verification-before-completion` |
| 启动应用手工验证 | `run` |
| 存在 2 个以上互相独立的子任务 | `superpowers:dispatching-parallel-agents` |
| 修改本 CLAUDE.md | `claude-md-management:claude-md-improver` |
