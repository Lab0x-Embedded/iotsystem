#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("IoT Device Manager");
    a.setOrganizationName("E2IoT");

    QFile styleFile(":/styles/dark.css");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&styleFile);
        a.setStyleSheet(in.readAll());
        styleFile.close();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
