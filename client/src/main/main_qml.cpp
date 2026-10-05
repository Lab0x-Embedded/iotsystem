#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "models/rulemodel.h"
#include "models/groupmodel.h"
#include "main/datamanager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("IoT Device Manager");
    app.setOrganizationName("E2IoT");
    app.setApplicationVersion("2.0");

    QQuickStyle::setStyle("Basic");  // QtShadcn 要求 Basic style（自绘 token）

    DataManager dataManager;

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("dataManager", &dataManager);
    engine.rootContext()->setContextProperty("deviceModel", dataManager.deviceModel());
    engine.rootContext()->setContextProperty("alarmModel", dataManager.alarmModel());
    engine.rootContext()->setContextProperty("groupModel", dataManager.groupModel());
    engine.rootContext()->setContextProperty("ruleModel", dataManager.ruleModel());

    // QtShadcn QML 模块导入路径 (build 目录下 third_party 产物)
#ifdef QTSHADCN_IMPORT_PATH
    engine.addImportPath(QStringLiteral(QTSHADCN_IMPORT_PATH));
#endif

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
