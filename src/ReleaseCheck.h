#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

class ReleaseCheck : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool upToDate READ upToDate NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY changed)
    Q_PROPERTY(qreal installProgress READ installProgress NOTIFY changed)

public:
    explicit ReleaseCheck(QObject *parent = nullptr);
    bool updateAvailable() const { return !download_.isEmpty(); }
    QString status() const { return status_; }
    bool checking() const { return checking_; }
    bool upToDate() const { return upToDate_; }
    bool canInstall() const;
    bool busy() const { return busy_; }
    bool installing() const { return installing_; }
    qreal downloadProgress() const { return downloadProgress_; }
    qreal installProgress() const { return installProgress_; }
    Q_INVOKABLE void check();
    Q_INVOKABLE void openUpdate();

signals:
    void changed();

private:
    void finishUpdate(const QString &message);
    void startInstaller();
    void readInstallProgress();

    QNetworkAccessManager network_;
    QUrl download_;
    QString assetName_;
    QString version_;
    QByteArray digest_;
    qint64 assetSize_ = 0;
    QString status_;
    QString packagePath_;
    QByteArray aptOutput_;
    std::unique_ptr<QTemporaryDir> staging_;
    QProcess installer_;
    qreal downloadProgress_ = 0;
    qreal installProgress_ = -1;
    bool checking_ = false;
    bool upToDate_ = false;
    bool busy_ = false;
    bool installing_ = false;
};
