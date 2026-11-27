#include "ReleaseCheck.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>
#include <QVersionNumber>
#include <algorithm>

namespace {
constexpr qint64 maxPackageSize = 32 * 1024 * 1024;
}

ReleaseCheck::ReleaseCheck(QObject *parent) : QObject(parent)
{
    connect(&installer_, &QProcess::readyReadStandardOutput, this, &ReleaseCheck::readInstallProgress);
    connect(&installer_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finishUpdate(tr("Could not start the system installer."));
    });
    connect(&installer_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus result) {
        if (result != QProcess::NormalExit || code != 0) {
            qWarning().noquote() << installer_.readAllStandardError();
            finishUpdate(code == 126 ? tr("Installation was cancelled.")
                                     : tr("Installation failed. Check your system package manager and try again."));
            return;
        }

        const QString pid = QString::number(QCoreApplication::applicationPid());
        const bool restarting = QProcess::startDetached("/bin/sh", {
            "-c",
            "i=0; while kill -0 \"$1\" 2>/dev/null; do "
            "i=$((i + 1)); [ \"$i\" -lt 100 ] || exit 1; sleep 0.1; done; exec /usr/bin/clipify",
            "clipify-restart", pid
        });
        finishUpdate(restarting ? tr("Update installed. Restarting Clipify…")
                                : tr("Update installed. Close and reopen Clipify to finish."));
        if (restarting)
            QCoreApplication::exit(0);
    });
}

bool ReleaseCheck::canInstall() const
{
    return !assetName_.isEmpty() && digest_.size() == 32 && assetSize_ > 0
        && assetSize_ <= maxPackageSize && QCoreApplication::applicationFilePath() == "/usr/bin/clipify"
        && QFileInfo("/usr/bin/pkexec").isExecutable() && QFileInfo("/usr/bin/apt-get").isExecutable();
}

void ReleaseCheck::check()
{
    if (checking_ || busy_)
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
            assetName_.clear();
            digest_.clear();
            version_.clear();
            assetSize_ = 0;
            if (latest > installed) {
                version_ = version;
                const QString releases = "https://github.com/riccify/Clipify/releases/";
                download_ = QUrl(releases + "tag/" + tag);
#ifdef Q_OS_LINUX
                if (QSysInfo::currentCpuArchitecture() == "x86_64") {
                    const QString file = "clipify_" + version + "_amd64.deb";
                    const auto assets = release.value("assets").toArray();
                    for (const auto &asset : assets) {
                        if (asset.toObject().value("name").toString() == file) {
                            download_ = QUrl(releases + "download/" + tag + '/' + file);
                            const auto assetInfo = asset.toObject();
                            const QString hash = assetInfo.value("digest").toString();
                            static const QRegularExpression sha256("^sha256:([a-fA-F0-9]{64})$");
                            const auto hashMatch = sha256.match(hash);
                            if (hashMatch.hasMatch())
                                digest_ = QByteArray::fromHex(hashMatch.captured(1).toLatin1());
                            assetName_ = file;
                            assetSize_ = assetInfo.value("size").toInteger();
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
    if (!updateAvailable() || busy_ || checking_)
        return;
    if (!canInstall()) {
        if (!QDesktopServices::openUrl(download_)) {
            status_ = tr("Could not open the download in your browser.");
            emit changed();
        }
        return;
    }

    busy_ = true;
    downloadProgress_ = 0;
    installProgress_ = -1;
    status_ = tr("Downloading Clipify %1…").arg(version_);
    emit changed();

    QNetworkRequest request(download_);
    request.setRawHeader("User-Agent", "Clipify");
    request.setTransferTimeout(30000);
    auto *reply = network_.get(request);
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, data] {
        const auto chunk = reply->read(assetSize_ - data->size() + 1);
        if (chunk.size() > assetSize_ - data->size()) {
            reply->abort();
            return;
        }
        data->append(chunk);
        downloadProgress_ = qreal(data->size()) / assetSize_;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || data->size() != assetSize_
            || QCryptographicHash::hash(*data, QCryptographicHash::Sha256) != digest_) {
            finishUpdate(tr("Download failed verification. Try again."));
            return;
        }

        staging_ = std::make_unique<QTemporaryDir>();
        if (!staging_->isValid()) {
            finishUpdate(tr("Could not create a temporary update folder."));
            return;
        }
        packagePath_ = staging_->filePath(assetName_);
        QSaveFile package(packagePath_);
        if (!package.open(QIODevice::WriteOnly) || package.write(*data) != data->size() || !package.commit()) {
            finishUpdate(tr("Could not save the update package."));
            return;
        }
        startInstaller();
    });
}

void ReleaseCheck::startInstaller()
{
    QProcess metadata;
    metadata.start("/usr/bin/dpkg-deb", {"--show", "--showformat=${Package}\n${Version}\n${Architecture}\n", packagePath_});
    const QByteArray expected = "clipify\n" + version_.toUtf8() + "\namd64\n";
    if (!metadata.waitForFinished(3000) || metadata.exitStatus() != QProcess::NormalExit
        || metadata.exitCode() != 0 || metadata.readAllStandardOutput() != expected) {
        if (metadata.state() != QProcess::NotRunning) {
            metadata.kill();
            metadata.waitForFinished();
        }
        finishUpdate(tr("The downloaded package does not match this Clipify update."));
        return;
    }

    installing_ = true;
    aptOutput_.clear();
    status_ = tr("Waiting for installation authorization…");
    emit changed();
    installer_.start("/usr/bin/pkexec", {
        "--disable-internal-agent", "/usr/bin/apt-get", "--yes", "--no-remove",
        "-o", "APT::Status-Fd=1", "install", packagePath_
    });
}

void ReleaseCheck::readInstallProgress()
{
    aptOutput_ += installer_.readAllStandardOutput();
    for (qsizetype end = aptOutput_.indexOf('\n'); end >= 0; end = aptOutput_.indexOf('\n')) {
        const auto line = aptOutput_.left(end);
        aptOutput_.remove(0, end + 1);
        if (!line.startsWith("pmstatus:"))
            continue;

        const auto fields = line.split(':');
        if (fields.size() >= 4) {
            bool valid = false;
            qreal percent = fields.at(2).toDouble(&valid);
            if (!valid && fields.size() >= 5)
                percent = fields.at(3).toDouble(&valid);
            if (valid) {
                installProgress_ = std::clamp(percent / 100, qreal(0), qreal(1));
                status_ = tr("Installing Clipify %1…").arg(version_);
                emit changed();
            }
        }
    }
}

void ReleaseCheck::finishUpdate(const QString &message)
{
    staging_.reset();
    busy_ = false;
    installing_ = false;
    status_ = message;
    emit changed();
}
