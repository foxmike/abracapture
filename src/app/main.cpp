#include "AppController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char *argv[]) {
    QGuiApplication application(argc, argv);
    AppController appController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("appController", &appController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Abracapture/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
