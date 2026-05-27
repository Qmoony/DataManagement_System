#include "MainWindow.h"

#include "SqlHighlighter.h"
#include "sql/Executor.h"
#include "sql/Lexer.h"
#include "sql/Parser.h"
#include "sql/SqlError.h"

#include <QApplication>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("数据管理系统 — 邱建勇"));
    resize(1080, 720);
    setMinimumSize(960, 640);
    buildUi();
    setupShortcuts();
    renderEmpty(QStringLiteral("等待你输入 SQL · 试试 SELECT * FROM studentTable"));
    setStatus(QStringLiteral("就绪 — Enter 执行,Shift+Enter 换行,Ctrl+L 清空"));
}

// ---------------- UI 构建 ----------------

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(32, 28, 32, 24);
    root->setSpacing(18);

    buildHeader(root);
    buildContent(root);
    buildPagination(root);
    buildInputBar(root);
}

void MainWindow::buildHeader(QVBoxLayout* parent) {
    auto* wrap = new QWidget(this);
    auto* col = new QVBoxLayout(wrap);
    col->setContentsMargins(0, 8, 0, 4);
    col->setSpacing(10);

    headerTitle_ = new QLabel(QStringLiteral("欢迎来到邱建勇的数据管理系统"), this);
    headerTitle_->setObjectName("headerTitle");
    headerTitle_->setAlignment(Qt::AlignCenter);
    col->addWidget(headerTitle_);

    auto* ruleRow = new QHBoxLayout;
    ruleRow->addStretch();
    auto* rule = new QFrame(this);
    rule->setObjectName("headerRule");
    rule->setFixedSize(60, 2);
    ruleRow->addWidget(rule);
    ruleRow->addStretch();
    col->addLayout(ruleRow);

    auto* subtle = new QLabel(
        QStringLiteral("D A T A   S T R U C T U R E S   ·   C S V   ·   S Q L"), this);
    subtle->setObjectName("headerSubtle");
    subtle->setAlignment(Qt::AlignCenter);
    col->addWidget(subtle);

    parent->addWidget(wrap);
}

void MainWindow::buildContent(QVBoxLayout* parent) {
    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName("contentStack");

    // 索引 0:结果表格
    table_ = new QTableWidget(contentStack_);
    table_->setObjectName("resultTable");
    table_->setColumnCount(0);
    table_->setRowCount(0);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setDefaultSectionSize(40);
    table_->verticalHeader()->setVisible(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setMouseTracking(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentStack_->addWidget(table_);

    // 索引 1:空状态
    emptyState_ = new QWidget(contentStack_);
    emptyState_->setObjectName("emptyState");
    auto* emptyLayout = new QVBoxLayout(emptyState_);
    emptyLayout->setContentsMargins(0, 24, 0, 24);
    emptyLayout->setSpacing(18);
    emptyLayout->addStretch();

    emptyStateSvg_ = new QLabel(emptyState_);
    emptyStateSvg_->setObjectName("emptyStateSvg");
    emptyStateSvg_->setAlignment(Qt::AlignCenter);
    // 将 SVG 渲染为透明背景 QPixmap
    {
        QSvgRenderer renderer(QStringLiteral(":/empty.svg"));
        const int w = 200, h = 160;
        QPixmap pm(w, h);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        renderer.render(&painter);
        painter.end();
        emptyStateSvg_->setPixmap(pm);
        emptyStateSvg_->setFixedSize(w, h);
    }
    auto* svgRow = new QHBoxLayout;
    svgRow->addStretch();
    svgRow->addWidget(emptyStateSvg_);
    svgRow->addStretch();
    emptyLayout->addLayout(svgRow);

    emptyStateMsg_ = new QLabel(QStringLiteral(""), emptyState_);
    emptyStateMsg_->setObjectName("emptyStateMsg");
    emptyStateMsg_->setAlignment(Qt::AlignCenter);
    emptyStateMsg_->setWordWrap(true);
    emptyLayout->addWidget(emptyStateMsg_);

    emptyLayout->addStretch();
    contentStack_->addWidget(emptyState_);

    // 透明度动画(用于翻页与状态切换的淡入)
    contentOpacity_ = new QGraphicsOpacityEffect(contentStack_);
    contentOpacity_->setOpacity(1.0);
    contentStack_->setGraphicsEffect(contentOpacity_);

    contentAnim_ = new QPropertyAnimation(contentOpacity_, "opacity", this);
    contentAnim_->setDuration(220);
    contentAnim_->setEasingCurve(QEasingCurve::OutCubic);

    parent->addWidget(contentStack_, /*stretch=*/1);
}

void MainWindow::buildPagination(QVBoxLayout* parent) {
    paginationBar_ = new QWidget(this);
    paginationBar_->setObjectName("paginationBar");

    auto* row = new QHBoxLayout(paginationBar_);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(14);
    row->addStretch();

    prevBtn_ = new QPushButton(QStringLiteral("< 上一页"), paginationBar_);
    prevBtn_->setObjectName("pageButton");
    prevBtn_->setCursor(Qt::PointingHandCursor);
    prevBtn_->setFocusPolicy(Qt::NoFocus);
    row->addWidget(prevBtn_);

    pageLabel_ = new QLabel(QStringLiteral("第 0 / 0 页 · 共 0 行"), paginationBar_);
    pageLabel_->setObjectName("pageLabel");
    pageLabel_->setAlignment(Qt::AlignCenter);
    pageLabel_->setMinimumWidth(220);
    row->addWidget(pageLabel_);

    nextBtn_ = new QPushButton(QStringLiteral("下一页 >"), paginationBar_);
    nextBtn_->setObjectName("pageButton");
    nextBtn_->setCursor(Qt::PointingHandCursor);
    nextBtn_->setFocusPolicy(Qt::NoFocus);
    row->addWidget(nextBtn_);

    row->addStretch();

    paginationBar_->setVisible(false);
    parent->addWidget(paginationBar_);

    connect(prevBtn_, &QPushButton::clicked, this, &MainWindow::onPrevPage);
    connect(nextBtn_, &QPushButton::clicked, this, &MainWindow::onNextPage);
}

void MainWindow::buildInputBar(QVBoxLayout* parent) {
    auto* bar = new QHBoxLayout;
    bar->setSpacing(12);

    input_ = new QPlainTextEdit(this);
    input_->setObjectName("sqlInput");
    input_->setPlaceholderText(QStringLiteral(
        "输入 SQL,Enter 执行 · Shift+Enter 换行  例:select * from studentTable where class_id = 1;"));
    input_->setTabChangesFocus(true);
    input_->setLineWrapMode(QPlainTextEdit::NoWrap);
    input_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input_->setFixedHeight(72);
    input_->installEventFilter(this);
    bar->addWidget(input_, /*stretch=*/1);

    runBtn_ = new QPushButton(QStringLiteral("执 行"), this);
    runBtn_->setObjectName("runButton");
    runBtn_->setCursor(Qt::PointingHandCursor);
    bar->addWidget(runBtn_);

    parent->addLayout(bar);

    connect(runBtn_, &QPushButton::clicked, this, &MainWindow::onExecute);

    // 关键字 / 字符串 / 数字着色
    highlighter_ = new SqlHighlighter(input_->document());
}

void MainWindow::setupShortcuts() {
    auto bind = [this](const QKeySequence& seq, auto slot) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, slot);
    };
    bind(QKeySequence(Qt::CTRL | Qt::Key_Return),  &MainWindow::onExecute);
    bind(QKeySequence(Qt::CTRL | Qt::Key_Enter),   &MainWindow::onExecute);
    bind(QKeySequence(Qt::Key_F5),                 &MainWindow::onExecute);
    bind(QKeySequence(Qt::CTRL | Qt::Key_L),       &MainWindow::onClearInput);
    bind(QKeySequence(Qt::ALT  | Qt::Key_Right),   &MainWindow::onNextPage);
    bind(QKeySequence(Qt::ALT  | Qt::Key_Left),    &MainWindow::onPrevPage);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == input_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const int key = ke->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) {
                return false; // 让 QPlainTextEdit 默认插入换行
            }
            onExecute();
            return true;
        }
        if (key == Qt::Key_Escape) {
            input_->clearFocus();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// ---------------- 数据目录 ----------------

QString MainWindow::dataDir() const {
    QString p = QCoreApplication::applicationDirPath() + "/data";
    if (QDir(p).exists()) return p;
    QString p2 = QCoreApplication::applicationDirPath() + "/../data";
    if (QDir(p2).exists()) return QDir(p2).absolutePath();
    return p;
}

// ---------------- 执行 ----------------

void MainWindow::onExecute() {
    QString sql = input_->toPlainText().trimmed();
    if (sql.isEmpty()) {
        showError(QStringLiteral("提示"), QStringLiteral("请先输入要执行的 SQL 语句"));
        return;
    }
    hasExecuted_ = true;
    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto   ast = parser.parse();

        Executor exe(dataDir());
        auto     res = exe.execute(ast.get());

        if (res.operation == "SELECT") {
            renderResult(res.resultTable);
            setStatus(QStringLiteral("SELECT 完成 · 返回 %1 行").arg(res.affectedRows));
        } else {
            clearResult();
            renderEmpty(QStringLiteral("%1 已完成 · 影响 %2 行")
                            .arg(res.operation).arg(res.affectedRows));
            setStatus(QStringLiteral("%1 完成 · 影响 %2 行")
                          .arg(res.operation).arg(res.affectedRows));
            QMessageBox box(this);
            box.setWindowTitle(QStringLiteral("执行成功"));
            box.setText(QStringLiteral("%1 已执行,影响 %2 行。")
                            .arg(res.operation).arg(res.affectedRows));
            box.setIcon(QMessageBox::Information);
            box.exec();
        }
    } catch (const SqlError& e) {
        QString detail = e.message();
        if (e.position() > 0) {
            detail += QStringLiteral("\n位置:第 %1 列").arg(e.position());
        }
        showError(QStringLiteral("SQL 错误"), detail, e.position());
    } catch (const std::exception& e) {
        showError(QStringLiteral("执行错误"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::onPrevPage() {
    if (!paginationBar_->isVisible() || !prevBtn_->isEnabled()) return;
    if (currentPage_ > 0) {
        --currentPage_;
        refreshPage();
    }
}

void MainWindow::onNextPage() {
    if (!paginationBar_->isVisible() || !nextBtn_->isEnabled()) return;
    const int total = static_cast<int>(cachedResult_.rows.size());
    const int totalPages = std::max(1, (total + kPageSize - 1) / kPageSize);
    if (currentPage_ < totalPages - 1) {
        ++currentPage_;
        refreshPage();
    }
}

void MainWindow::onClearInput() {
    input_->clear();
    input_->setFocus();
}

// ---------------- 渲染 ----------------

void MainWindow::renderResult(const Table& t) {
    cachedResult_ = t;
    currentPage_ = 0;
    refreshPage();
}

void MainWindow::refreshPage() {
    const int total = cachedResult_.rows.size();

    if (total == 0) {
        clearResult();
        renderEmpty(QStringLiteral("查询完成,没有匹配的数据"));
        paginationBar_->setVisible(false);
        return;
    }

    const int totalPages = std::max(1, (total + kPageSize - 1) / kPageSize);
    currentPage_ = std::clamp(currentPage_, 0, totalPages - 1);

    const int begin = currentPage_ * kPageSize;
    const int end   = std::min(begin + kPageSize, total);
    const int sliceRows = end - begin;

    table_->clear();
    table_->setColumnCount(cachedResult_.columns.size());
    table_->setRowCount(sliceRows);

    QStringList headers;
    headers.reserve(cachedResult_.columns.size());
    for (const auto& c : cachedResult_.columns) headers << c.toUpper();
    table_->setHorizontalHeaderLabels(headers);

    // 行头显示真实序号(1-based,跨页连续)
    QStringList vheaders;
    vheaders.reserve(sliceRows);
    for (int i = 0; i < sliceRows; ++i) {
        vheaders << QString::number(begin + i + 1);
    }
    table_->setVerticalHeaderLabels(vheaders);

    for (int r = 0; r < sliceRows; ++r) {
        const QStringList& row = cachedResult_.rows[begin + r];
        for (int c = 0; c < cachedResult_.columns.size(); ++c) {
            auto* item = new QTableWidgetItem(row.value(c));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table_->setItem(r, c, item);
        }
    }
    for (int c = 0; c < cachedResult_.columns.size() - 1; ++c) {
        table_->resizeColumnToContents(c);
        int w = table_->columnWidth(c);
        table_->setColumnWidth(c, std::max(w + 24, 100));
    }

    pageLabel_->setText(QStringLiteral("第 %1 / %2 页 · 共 %3 行")
                           .arg(currentPage_ + 1).arg(totalPages).arg(total));
    prevBtn_->setEnabled(currentPage_ > 0);
    nextBtn_->setEnabled(currentPage_ < totalPages - 1);
    paginationBar_->setVisible(true);

    contentStack_->setCurrentIndex(0);
    animateContentFadeIn();
}

void MainWindow::renderEmpty(const QString& message) {
    emptyStateMsg_->setText(message);
    contentStack_->setCurrentIndex(1);
    paginationBar_->setVisible(false);
    animateContentFadeIn();
}

void MainWindow::clearResult() {
    cachedResult_ = Table{};
    currentPage_ = 0;
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);
}

void MainWindow::showError(const QString& title, const QString& msg, int /*pos*/) {
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setText(msg);
    box.setIcon(QMessageBox::Warning);
    box.exec();
    setStatus(QStringLiteral("✗ %1").arg(msg.split('\n').first()));
}

void MainWindow::setStatus(const QString& text) {
    statusBar()->showMessage(text);
}

// ---------------- 动效 ----------------

void MainWindow::animateContentFadeIn() {
    contentAnim_->stop();
    contentAnim_->setStartValue(0.35);
    contentAnim_->setEndValue(1.0);
    contentAnim_->start();
}
