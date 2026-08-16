#include "NotificationService.hpp"

#include <QMetaObject>
#include <QProcess>
#include <QStringList>
#include <QThread>

NotificationService::NotificationService(QObject* const parent)
    : QObject(parent) {}

bool NotificationService::isEnabled() const noexcept {
    return enabled_;
}

bool NotificationService::enabled() const noexcept {
    return enabled_;
}

void NotificationService::setEnabled(const bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    emit enabledChanged(enabled_);
}

void NotificationService::onCaptureStarted(QString sourceName) {
    if (!enabled_) {
        return;
    }
    const QString trimmedSource = sourceName.trimmed();
    const QString body = trimmedSource.isEmpty()
                             ? QStringLiteral("Audio capture in progress.")
                             : QStringLiteral("Capturing audio from %1.").arg(trimmedSource);
    sendNotification(QStringLiteral("Capture Started"),
                     body,
                     QStringLiteral("media-record"),
                     QStringLiteral("normal"));
}

void NotificationService::onCaptureStopped(QString filePath, const double durationSecs) {
    if (!enabled_) {
        return;
    }
    const QString trimmedPath = filePath.trimmed();
    const QString durationText = durationSecs >= 0.0
                                     ? QStringLiteral("%1s").arg(QString::number(durationSecs, 'f', 1))
                                     : QString();
    QString body;
    if (!trimmedPath.isEmpty() && !durationText.isEmpty()) {
        body = QStringLiteral("Recorded %1 to %2.").arg(durationText, trimmedPath);
    } else if (!trimmedPath.isEmpty()) {
        body = QStringLiteral("Saved recording to %1.").arg(trimmedPath);
    } else if (!durationText.isEmpty()) {
        body = QStringLiteral("Capture completed (%1).").arg(durationText);
    } else {
        body = QStringLiteral("Audio capture stopped.");
    }

    sendNotification(QStringLiteral("Capture Stopped"),
                     body,
                     QStringLiteral("media-playback-stop"),
                     QStringLiteral("normal"));
}

void NotificationService::onCaptureError(QString errorMessage) {
    if (!enabled_) {
        return;
    }
    const QString trimmedError = errorMessage.trimmed();
    const QString body = trimmedError.isEmpty()
                             ? QStringLiteral("An unexpected error occurred during capture.")
                             : trimmedError;
    sendNotification(QStringLiteral("Capture Error"),
                     body,
                     QStringLiteral("dialog-error"),
                     QStringLiteral("critical"));
}

void NotificationService::sendNotification(const QString& summary,
                                          const QString& body,
                                          const QString& icon,
                                          const QString& urgency) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, summary, body, icon, urgency] {
                sendNotification(summary, body, icon, urgency);
            },
            Qt::QueuedConnection);
        return;
    }

    QStringList arguments;
    arguments << QStringLiteral("-a") << QStringLiteral("Abracapture");
    if (!urgency.isEmpty()) {
        arguments << QStringLiteral("-u") << urgency;
    }
    if (!icon.isEmpty()) {
        arguments << QStringLiteral("-i") << icon;
    }
    arguments << summary << body;

    QProcess::startDetached(QStringLiteral("notify-send"), arguments);
}

#include "moc_NotificationService.cpp"
