# 数据管理系统

> 数据结构课程设计 · 邱建勇

基于 Qt6 的桌面 GUI 应用，内置手写的迷你 SQL 引擎，对四张 CSV 表（班级 / 学生 / 课程 / 成绩）执行 SELECT / INSERT / UPDATE / DELETE 操作。

---

## 功能特性

- **SQL 编辑器**：语法高亮（关键字大小写不敏感）、错误提示（精确到列位置，红色波浪下划线标记出错 token）
- **Schema 树面板**：左侧列出全部表及列名；右键菜单可一键生成 `SELECT *`、`INSERT INTO` 模板，或直接复制表名
- **多表 JOIN**：支持链式 `INNER JOIN ... ON`，以及 `FROM t1, t2, t3` 隐式 JOIN（逗号分隔，WHERE 负责过滤）
- **分页渲染**：SELECT 结果每页 20 行，翻页不重跑查询；结果切换带淡入动画
- **执行计时**：运行时按钮显示 `...`，完成后状态栏展示耗时（ms）
- **欢迎 / 空状态插画**：首次打开渲染 `welcome.svg`，查询后无数据渲染 `empty.svg`
- **即时落盘**：INSERT / UPDATE / DELETE 修改后立即写回 CSV

## 语法高亮配色

| 元素 | 样式 |
|---|---|
| 关键字（`SELECT`、`WHERE` 等） | 珊瑚红，加粗 |
| 数字 | 柔和绿 |
| 运算符（`=`、`<`、`>`、`!=` 等） | 淡紫 |
| 表名（四张 CSV 表） | 柔金 |
| 列名（已知 Schema 列） | 浅蓝 |
| 字符串字面量 | 柔金 |
| 注释（`--` 行注释） | 灰色斜体 |
| 出错 token | 红色波浪下划线 |

## 支持的 SQL 方言

```sql
-- 多表联查
SELECT s.name, c.name, sc.score
  FROM studentTable s
  JOIN scoreTable sc ON sc.stu_id = s.id
  JOIN courseTable c  ON c.id = sc.course_id
 WHERE sc.score >= 90
 ORDER BY sc.score DESC;

-- 隐式 JOIN（逗号多表，WHERE 过滤）
SELECT s.name, sc.score
  FROM studentTable s, scoreTable sc
 WHERE sc.stu_id = s.id AND sc.score >= 90;

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

**依赖：** Qt 6.x · CLion 2023+ · Qt 自带 MinGW 13.1.0
（**不要**使用系统 MinGW / MSVC，ABI 不兼容）

用 CLion 直接打开根目录，`.idea/` 中的 CMake profile 和工具链配置会自动加载：

1. `File → Open` 选择项目根目录
2. 工具栏锤子按钮编译（或 `Build → Build Project`）
3. 选择 `DataManagement_System` 运行配置，点击 ▶

> post-build 步骤会自动把 CSV 数据和 Qt 运行时拷贝到构建目录，无需手动操作。

<details>
<summary>命令行备用（不使用 CLion 时）</summary>

```powershell
cmake -S . -B cmake-build-debug -G "MinGW Makefiles"
cmake --build cmake-build-debug
.\cmake-build-debug\DataManagement_System.exe
```

</details>

## 架构

```
QSplitter（水平分割）
├── QTreeWidget (Schema 树)    表 / 列浏览，右键生成 SQL 模板
└── QVBoxLayout（右侧内容区）
    ├── QPlainTextEdit (SQL 输入 + SqlHighlighter 着色)
    │     → Lexer     词法分析，关键字大小写不敏感
    │     → Parser    手写递归下降，产 AST
    │     → Executor  解释执行；CsvIO 懒加载并缓存到 QHash<QString,Table>
    │     → CsvIO     RFC4180 风格 UTF-8 读写
    ├── QStackedWidget
    │   ├── QTableWidget   分页结果（kPageSize=20），切换带淡入动画
    │   └── 空状态面板     welcome.svg（初始）/ empty.svg（查询无结果）
    └── 状态栏             执行耗时（QElapsedTimer，ms 精度）
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
├── resources/
│   ├── welcome.svg   # 欢迎插画（初始空状态）
│   ├── empty.svg     # 空结果插画（查询后无数据）
│   ├── resources.qrc
│   └── style.qss     # 全局 QSS 主题
├── CMakeLists.txt
└── README.md
```
