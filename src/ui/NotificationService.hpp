#pragma once

#include <QObject>
#include <QString>

class NotificationService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit NotificationService(QObject* parent = nullptr);
    ~NotificationService() override = default;

    [[nodiscard]] bool isEnabled() const noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    void setEnabled(bool enabled);

public slots:
    void onCaptureStarted(QString sourceName);
    void onCaptureStopped(QString filePath, double durationSecs);
    void onCaptureError(QString errorMessage);

signals:
    void enabledChanged(bool enabled);

private:
    void sendNotification(const QString& summary,
                          const QString& body,
                          const QString& icon = QString(),
                          const QString& urgency = QStringLiteral("normal"));

    bool enabled_ = true;
};
