#include "MainWindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("DataManagement_System");
    QApplication::setOrganizationName("QiuJianyong");

    QFile qss(":/style.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    MainWindow w;
    w.show();
    return app.exec();
}
