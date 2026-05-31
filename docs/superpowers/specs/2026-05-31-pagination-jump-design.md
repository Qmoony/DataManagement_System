# 分页跳转功能设计

**日期：** 2026-05-31
**状态：** 已批准

## 背景

当前分页栏只有"上一页 / 下一页"两个按钮，查询结果页数较多时（如 10+ 页）逐页点击效率低。用户需要一种直接输入目标页码跳转的方式。

## 目标

在分页栏添加一个页码输入框和"跳转"按钮，用户输入合法页码后按 Enter 或点击按钮即可直接跳转到对应页。

## 选定方案：QLineEdit + 跳转按钮（方案 B）

### UI 布局

在现有分页栏右侧增加一组控件，与翻页按钮之间用竖线分隔：

```
[stretch] [← 上一页]  [第 X / Y 页 · 共 Z 行]  [下一页 →]  │  [___]  [跳转]  [stretch]
```

新增控件：
- `QLineEdit* pageJumpEdit_`：宽 52px，`placeholderText("页码")`，居中对齐，`objectName("pageJumpEdit")`
- `QPushButton* jumpBtn_`：文字 "跳转"，`objectName("pageButton")`（复用已有 pageButton 样式）
- `QFrame` 竖线分隔（`QFrame::VLine`）

### 交互行为

- 触发跳转：在输入框按 **Enter** 或点击 **"跳转"按钮**
- 验证逻辑：解析为整数，范围 `[1, totalPages]`；无效输入（非数字、空、越界）静默清空输入框，不弹窗打断操作
- 跳转成功后：清空输入框（防止重复点击误跳）
- 分页栏隐藏时（无查询结果）：输入框和跳转按钮随之不可用（`paginationBar_->setVisible(false)` 已整体控制）

### 不在范围内

- 不添加键盘快捷键（Alt+数字等）
- 不在输入框实时显示当前页码
- 不添加错误提示 tooltip（越界时静默清空即可）

## 代码变更范围

| 文件 | 改动描述 |
|------|---------|
| [src/MainWindow.h](../../../src/MainWindow.h) | 新增成员：`QLineEdit* pageJumpEdit_`、`QPushButton* jumpBtn_`；新增槽声明：`void onJumpToPage()` |
| [src/MainWindow.cpp](../../../src/MainWindow.cpp) | `buildPagination()`：在 nextBtn_ 后添加竖线分隔、pageJumpEdit_、jumpBtn_，并连接信号；新增 `onJumpToPage()` 实现 |
| [resources/style.qss](../../../resources/style.qss) | 可选：为 `#pageJumpEdit` 指定宽度、内边距样式，与现有输入框风格保持一致 |

## 验证方式

1. 构建并运行，执行一条返回 20 行以上的 SELECT 查询（如 `SELECT * FROM studentTable`）
2. 确认分页栏出现输入框和"跳转"按钮
3. 输入合法页码（如 `2`）按 Enter —— 应跳转到第 2 页，输入框清空
4. 点击"跳转"按钮 —— 同上
5. 输入越界值（如 `999`）—— 应静默清空，页面不变
6. 输入非数字（如 `abc`）—— 同上
7. 翻到其他页后再用跳转框 —— 正常工作
