# Qt IoT Dashboard - IoTDeviceManager.pro
# Compatible with Qt 5.12+, recommended Qt 5.15 LTS / Qt 6.2+
QT       += core gui widgets charts network

TARGET   = IoTDeviceManager
TEMPLATE = app
CONFIG   += c++17

SOURCES += \
    src/main/main.cpp \
    src/main/mainwindow.cpp \
    src/main/navsidebar.cpp \
    src/main/datamanager.cpp \
    src/mock/mockdatasource.cpp \
    src/network/httpclient.cpp \
    src/models/devicemodel.cpp \
    src/models/alarmmodel.cpp \
    src/models/datapointmodel.cpp \
    src/pages/overviewpage.cpp \
    src/pages/detailpage.cpp \
    src/pages/dashboardpage.cpp \
    src/pages/alarmcenterpage.cpp \
    src/widgets/realtimechart.cpp \
    src/widgets/gaugewidget.cpp \
    src/widgets/topdevicestable.cpp \
    src/widgets/statusindicator.cpp \
    src/theme/theme.cpp

HEADERS += \
    src/main/mainwindow.h \
    src/main/navsidebar.h \
    src/main/datamanager.h \
    src/mock/mockdatasource.h \
    src/network/httpclient.h \
    src/models/devicemodel.h \
    src/models/alarmmodel.h \
    src/models/datapointmodel.h \
    src/pages/overviewpage.h \
    src/pages/detailpage.h \
    src/pages/dashboardpage.h \
    src/pages/alarmcenterpage.h \
    src/widgets/realtimechart.h \
    src/widgets/gaugewidget.h \
    src/widgets/topdevicestable.h \
    src/widgets/statusindicator.h \
    src/theme/theme.h

RESOURCES += resources/styles.qrc

DESTDIR     = build
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

INCLUDEPATH += src src/theme
QMAKE_CXXFLAGS += -Wall -Wextra
