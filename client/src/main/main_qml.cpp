#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "models/groupmodel.h"
#include "main/datamanager.h"
#include "theme/theme.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("IoT Device Manager");
    app.setOrganizationName("E2IoT");
    app.setApplicationVersion("2.0");

    QQuickStyle::setStyle("Material");

    DataManager dataManager;
    ThemeManager *themeManager = ThemeManager::instance();

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("dataManager", &dataManager);
    engine.rootContext()->setContextProperty("deviceModel", dataManager.deviceModel());
    engine.rootContext()->setContextProperty("alarmModel", dataManager.alarmModel());
    engine.rootContext()->setContextProperty("groupModel", dataManager.groupModel());
    engine.rootContext()->setContextProperty("themeManager", themeManager);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    dataManager.start();

    return app.exec();
}
