#include "AppController.hpp"

#include <QApplication>
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
    QFileDialog dialog(QApplication::activeWindow(),
                       QStringLiteral("Select Output Directory"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setDirectory(currentDirectory);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().constFirst();
}
