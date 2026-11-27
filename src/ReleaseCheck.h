#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class ReleaseCheck : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool upToDate READ upToDate NOTIFY changed)

public:
    explicit ReleaseCheck(QObject *parent = nullptr);
    bool updateAvailable() const { return !download_.isEmpty(); }
    QString status() const { return status_; }
    bool checking() const { return checking_; }
    bool upToDate() const { return upToDate_; }
    Q_INVOKABLE void check();
    Q_INVOKABLE void openUpdate();

signals:
    void changed();

private:
    QNetworkAccessManager network_;
    QUrl download_;
    QString status_;
    bool checking_ = false;
    bool upToDate_ = false;
};
