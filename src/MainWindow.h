#pragma once

#include "storage/Table.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QVBoxLayout;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class SqlHighlighter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onExecute();
    void onPrevPage();
    void onNextPage();
    void onJumpToPage();
    void onClearInput();
    void onSchemaContextMenu(const QPoint& pos);

private:
    void buildUi();
    void buildHeader(QVBoxLayout* parent);
    void buildMainArea(QVBoxLayout* parent);
    void buildContent(QVBoxLayout* parent);
    void buildPagination(QVBoxLayout* parent);
    void buildInputBar(QVBoxLayout* parent);
    void setupShortcuts();
    void populateSchema();

    void renderResult(const Table& t);
    void refreshPage();
    void renderEmpty(const QString& message);

    void clearResult();
    void showError(const QString& title, const QString& msg, int pos = -1);
    void setStatus(const QString& text);

    void animateContentFadeIn();

    QString dataDir() const;

    QLabel*           headerTitle_   = nullptr;

    QTreeWidget*      schemaTree_    = nullptr;

    QStackedWidget*   contentStack_  = nullptr;
    QTableWidget*     table_         = nullptr;
    QWidget*          emptyState_    = nullptr;
    QLabel*           emptyStateSvg_ = nullptr;
    QLabel*           emptyStateMsg_ = nullptr;

    QWidget*          paginationBar_ = nullptr;
    QPushButton*      prevBtn_       = nullptr;
    QPushButton*      nextBtn_       = nullptr;
    QLineEdit*        pageJumpEdit_  = nullptr;
    QPushButton*      jumpBtn_       = nullptr;
    QLabel*           pageLabel_     = nullptr;

    QPlainTextEdit*   input_         = nullptr;
    QPushButton*      runBtn_        = nullptr;
    SqlHighlighter*   highlighter_   = nullptr;

    QGraphicsOpacityEffect* contentOpacity_ = nullptr;
    QPropertyAnimation*     contentAnim_    = nullptr;

    Table cachedResult_;
    int   currentPage_ = 0;
    bool  hasExecuted_ = false;

    static constexpr int kPageSize = 20;
};
