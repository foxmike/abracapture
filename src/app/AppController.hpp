#pragma once

#include <QObject>
#include <QString>

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);

    QString appName() const;
    QString appVersion() const;
};
