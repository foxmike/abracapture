#include "AppController.hpp"

AppController::AppController(QObject *parent)
    : QObject(parent) {}

QString AppController::appName() const {
    return QStringLiteral("Abracapture");
}

QString AppController::appVersion() const {
    return QStringLiteral("0.1.0");
}
