#include "mainwindow.h"
#include "theme/theme.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("IoT Device Manager");
    a.setOrganizationName("E2IoT");

    ThemeManager::instance()->load();

    MainWindow w;
    w.show();
    return a.exec();
}
