#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QtWebEngine/QtWebEngine>

int main(int argc, char *argv[])
{
    QtWebEngine::initialize();

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/main-icon.png"));
    MainWindow w;
    w.show();
    return a.exec();
}
