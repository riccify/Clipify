#include "ReleaseCheck.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSysInfo>
#include <QVersionNumber>

ReleaseCheck::ReleaseCheck(QObject *parent) : QObject(parent)
{
}

void ReleaseCheck::check()
{
    if (checking_)
        return;

    checking_ = true;
    upToDate_ = false;
    status_ = tr("Checking for updates…");
    emit changed();

    QNetworkRequest request(QUrl("https://api.github.com/repos/riccify/Clipify/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Clipify");
    request.setTransferTimeout(8000);
    auto *reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto release = QJsonDocument::fromJson(reply->readAll()).object();
        static const QRegularExpression stableTag("^v?([0-9]+\\.[0-9]+\\.[0-9]+)$");
        const QString tag = release.value("tag_name").toString();
        const auto match = stableTag.match(tag);
        if (reply->error() != QNetworkReply::NoError || !match.hasMatch()
            || release.value("draft").toBool() || release.value("prerelease").toBool()) {
            status_ = tr("Could not check for updates.");
        } else {
            const QString version = match.captured(1);
            const auto installed = QVersionNumber::fromString(QCoreApplication::applicationVersion());
            const auto latest = QVersionNumber::fromString(version);
            download_.clear();
            if (latest > installed) {
                const QString releases = "https://github.com/riccify/Clipify/releases/";
                download_ = QUrl(releases + "tag/" + tag);
#ifdef Q_OS_LINUX
                if (QSysInfo::currentCpuArchitecture() == "x86_64") {
                    const QString file = "clipify_" + version + "_amd64.deb";
                    const auto assets = release.value("assets").toArray();
                    for (const auto &asset : assets) {
                        if (asset.toObject().value("name").toString() == file) {
                            download_ = QUrl(releases + "download/" + tag + '/' + file);
                            break;
                        }
                    }
                }
#endif
                status_ = tr("Version %1 is available.").arg(version);
            } else {
                upToDate_ = true;
                status_ = tr("Clipify is up to date.");
            }
        }
        checking_ = false;
        reply->deleteLater();
        emit changed();
    });
}

void ReleaseCheck::openUpdate()
{
    if (updateAvailable() && !QDesktopServices::openUrl(download_)) {
        status_ = tr("Could not open the download in your browser.");
        emit changed();
    }
}
