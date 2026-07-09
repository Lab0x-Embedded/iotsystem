#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "main/datamanager.h"
#include "theme/theme.h"

int main(int argc, char *argv[])
{
    // Use Qt Quick application
    QGuiApplication app(argc, argv);
    app.setApplicationName("IoT Device Manager");
    app.setOrganizationName("E2IoT");
    app.setApplicationVersion("2.0");

    // Set Quick Controls style
    QQuickStyle::setStyle("Material");

    // Create backend objects
    DataManager dataManager;
    ThemeManager *themeManager = ThemeManager::instance();

    // Create QML engine
    QQmlApplicationEngine engine;

    // Expose C++ objects to QML
    engine.rootContext()->setContextProperty("dataManager", &dataManager);
    engine.rootContext()->setContextProperty("deviceModel", dataManager.deviceModel());
    engine.rootContext()->setContextProperty("alarmModel", dataManager.alarmModel());
    engine.rootContext()->setContextProperty("themeManager", themeManager);

    // Load main QML file from resources
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Start data manager
    dataManager.start();

    return app.exec();
}
