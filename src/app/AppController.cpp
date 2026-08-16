#include "AppController.hpp"

#include <QFileDialog>

AppController::AppController(QObject *parent)
    : QObject(parent) {}

QString AppController::appName() const {
    return QStringLiteral("Abracapture");
}

QString AppController::appVersion() const {
    return QStringLiteral("0.1.0");
}

QString AppController::selectOutputDirectory(const QString& currentDirectory) const {
    return QFileDialog::getExistingDirectory(nullptr,
                                              QStringLiteral("Select Output Directory"),
                                              currentDirectory);
}
