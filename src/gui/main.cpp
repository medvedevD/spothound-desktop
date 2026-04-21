#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QtWebEngine/QtWebEngine>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--no-sandbox");
#endif
    QtWebEngine::initialize();

    QApplication a(argc, argv);
    a.setOrganizationName("Spothound");
    a.setApplicationName("Spothound");
    a.setWindowIcon(QIcon(":/icons/main-icon.png"));
    MainWindow w;
    w.show();
    return a.exec();
}
