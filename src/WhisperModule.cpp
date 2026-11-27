#include "WhisperModule.h"
#include "AppConfig.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QQuickWindow>
#include <QScreen>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <algorithm>

#ifdef Q_OS_LINUX
#include <csignal>
#include <unistd.h>
#endif

namespace {
constexpr auto modelKey = "whisper/modelPath";
constexpr auto languageKey = "whisper/language";
constexpr auto holdKey = "whisper/holdToTalk";

QString whisperExecutable()
{
    const QString applicationFolder = QCoreApplication::applicationDirPath();
    QString result = QStandardPaths::findExecutable("whisper-cli", {applicationFolder, "/usr/lib/clipify"});
    if (result.isEmpty())
        result = QStandardPaths::findExecutable("whisper-cli");
    return result;
}

QString defaultModel()
{
    constexpr auto name = "whisper/ggml-base.bin";
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    const auto userModel = data.filePath(name);
    const auto applicationModel = QDir(QCoreApplication::applicationDirPath()).filePath(name);
    for (const auto &candidate : {userModel, applicationModel, QStringLiteral("/usr/share/clipify/") + name}) {
        if (QFileInfo(candidate).isReadable())
            return candidate;
    }
    return userModel;
}

QString processDetail(QProcess &process)
{
    return QString::fromLocal8Bit(process.readAllStandardError().right(1200)).trimmed();
}
}

WhisperModule::WhisperModule(QObject *parent) : QObject(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, &WhisperModule::settingsChanged);
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    database_ = QSqlDatabase::addDatabase("QSQLITE", "whisper-history");
    database_.setDatabaseName(data.filePath("dictation.sqlite3"));
    if (QDir().mkpath(data.absolutePath()) && database_.open()) {
        QSqlQuery schema(database_);
        if (!schema.exec("CREATE TABLE IF NOT EXISTS dictations "
                         "(id INTEGER PRIMARY KEY, text TEXT NOT NULL, created_ms INTEGER NOT NULL)"))
            database_.close();
    }
    refreshHistory();

    recorder_.setStandardOutputFile(QProcess::nullDevice());
    whisper_.setStandardOutputFile(QProcess::nullDevice());
    stopTimeout_.setSingleShot(true);
    stopTimeout_.setInterval(4000);
    connect(&stopTimeout_, &QTimer::timeout, this, [this] {
        stopTimedOut_ = true;
        recorder_.kill();
    });
    connect(&recorder_, &QProcess::started, this, [this] {
        if (phase_ == Phase::Finishing)
            stopRecorder();
        else if (phase_ == Phase::Cancelling)
            recorder_.kill();
    });
    connect(&recorder_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(phase_ == Phase::Cancelling ? tr("Dictation cancelled.")
                                                : tr("Could not start microphone recording."),
                     phase_ != Phase::Cancelling);
    });
    connect(&recorder_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        stopTimeout_.stop();
        if (phase_ == Phase::Cancelling) {
            complete(tr("Dictation cancelled."));
        } else if (phase_ != Phase::Finishing || stopTimedOut_) {
            const QString detail = processDetail(recorder_);
            complete(detail.isEmpty() ? tr("Microphone recording stopped unexpectedly.")
                                      : tr("Microphone recording failed: %1").arg(detail), true);
        } else if (QFileInfo(temporary_->filePath("recording.wav")).size() < 8044) {
            complete(tr("The recording was too short. Try speaking before stopping."), true);
        } else {
            transcribe();
        }
    });
    connect(&whisper_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(phase_ == Phase::Cancelling ? tr("Dictation cancelled.")
                                                : tr("Could not start whisper-cli."),
                     phase_ != Phase::Cancelling);
    });
    connect(&whisper_, &QProcess::started, this, [this] {
        if (phase_ == Phase::Cancelling)
            whisper_.kill();
    });
    connect(&whisper_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (phase_ == Phase::Cancelling) {
            complete(tr("Dictation cancelled."));
            return;
        }

        QFile result(temporary_->filePath("transcript.txt"));
        if (status != QProcess::NormalExit || code != 0 || !result.open(QIODevice::ReadOnly)) {
            const QString detail = processDetail(whisper_);
            complete(detail.isEmpty() ? tr("Local transcription failed.")
                                      : tr("Local transcription failed: %1").arg(detail), true);
            return;
        }

        const QString transcript = QString::fromUtf8(result.readAll()).trimmed();
        result.close();
        if (transcript.isEmpty()) {
            complete(tr("No speech was transcribed."));
            return;
        }
        saveTranscript(transcript);
    });
}

QString WhisperModule::modelPath() const
{
    const QString selected = AppConfig::instance().value(modelKey).toString();
    return selected.isEmpty() ? defaultModel() : selected;
}

QString WhisperModule::language() const
{
    return AppConfig::instance().value(languageKey, "auto").toString();
}

bool WhisperModule::holdToTalk() const
{
    return AppConfig::instance().value(holdKey, false).toBool();
}

void WhisperModule::setHoldToTalk(bool enabled)
{
    if (AppConfig::instance().setValue(holdKey, enabled))
        emit settingsChanged();
}

void WhisperModule::chooseModel()
{
    const QString path = QFileDialog::getOpenFileName(nullptr, tr("Choose a Whisper model"),
                                QFileInfo(modelPath()).absolutePath(), tr("Whisper models (*.bin)"));
    if (!path.isEmpty() && AppConfig::instance().setValue(modelKey, path))
        emit settingsChanged();
}

void WhisperModule::useDefaultModel()
{
    if (AppConfig::instance().setValue(modelKey, QStringLiteral("")))
        emit settingsChanged();
}

void WhisperModule::setLanguage(const QString &language)
{
    if ((language == "auto" || language == "de" || language == "en")
        && AppConfig::instance().setValue(languageKey, language)) {
        emit settingsChanged();
    }
}

void WhisperModule::toggleRecording()
{
    if (phase_ == Phase::Idle) {
        startRecording();
    } else if (recording()) {
        setPhase(Phase::Finishing, tr("Finishing recording…"));
        if (recorder_.state() == QProcess::Running)
            stopRecorder();
    }
}

void WhisperModule::startRecording()
{
#ifdef Q_OS_LINUX
    executable_ = whisperExecutable();
    jobModel_ = modelPath();
    jobLanguage_ = language();
    if (executable_.isEmpty()) {
        complete(tr("Install whisper-cli to use local dictation. See Installer.md."), true);
        return;
    }
    const QFileInfo model(jobModel_);
    if (!model.isFile() || !model.isReadable()) {
        complete(tr("Install the default Whisper model or choose a model file."), true);
        return;
    }

    const QString pipewire = QStandardPaths::findExecutable("pw-record");
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const bool usePipewire = !pipewire.isEmpty() && !runtime.isEmpty()
                         && QFileInfo::exists(QDir(runtime).filePath("pipewire-0"));
    const QString capture = usePipewire ? pipewire : QStandardPaths::findExecutable("arecord");
    if (capture.isEmpty()) {
        complete(tr("Install PipeWire recording tools or ALSA arecord to use the microphone."), true);
        return;
    }

    temporary_ = std::make_unique<QTemporaryDir>(QDir::temp().filePath("clipify-dictation-XXXXXX"));
    if (!temporary_->isValid()) {
        complete(tr("Could not create a temporary recording folder."), true);
        return;
    }

    const QString output = temporary_->filePath("recording.wav");
    stopTimedOut_ = false;
    setPhase(Phase::Recording, tr("Recording…"));
    recorder_.start(capture, usePipewire
        ? QStringList{"--rate", "16000", "--channels", "1", "--format", "s16", output}
        : QStringList{"-q", "-t", "wav", "-f", "S16_LE", "-r", "16000", "-c", "1", output});
#else
    complete(tr("Microphone recording is available on Linux."), true);
#endif
}

void WhisperModule::stopRecorder()
{
#ifdef Q_OS_LINUX
    if (recorder_.processId() > 0)
        ::kill(static_cast<pid_t>(recorder_.processId()), SIGINT);
#endif
    stopTimeout_.start();
}

void WhisperModule::transcribe()
{
    const auto input = temporary_->filePath("recording.wav");
    const auto output = temporary_->filePath("transcript");
    setPhase(Phase::Transcribing, tr("Transcribing locally…"));
    whisper_.start(executable_, {"-m", jobModel_, "-f", input, "-l", jobLanguage_, "-ng",
                                "-t", QString::number(std::clamp(QThread::idealThreadCount(), 1, 8)),
                                "-otxt", "-of", output});
}

void WhisperModule::cancel()
{
    if (!busy() || phase_ == Phase::Cancelling)
        return;
    stopTimeout_.stop();
    setPhase(Phase::Cancelling, tr("Cancelling…"));
    if (recorder_.state() != QProcess::NotRunning)
        recorder_.kill();
    else if (whisper_.state() != QProcess::NotRunning)
        whisper_.kill();
}

void WhisperModule::placeOverlay(QQuickWindow *window) const
{
    auto *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const auto area = screen->availableGeometry();
    window->setScreen(screen);
    window->setPosition(area.center().x() - window->width() / 2,
                        std::max(area.top(), area.bottom() - window->height() - 28));
}

void WhisperModule::saveTranscript(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    emit transcriptReady();
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare("INSERT INTO dictations (text, created_ms) VALUES (?, ?)")) {
        query.addBindValue(text);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        if (query.exec()) {
            refreshHistory();
            complete(tr("Dictation copied."));
            return;
        }
    }
    complete(tr("Dictation copied, but it could not be saved to history."), true);
}

void WhisperModule::refreshHistory()
{
    QVariantList entries;
    QSqlQuery query(database_);
    if (database_.isOpen() && query.exec("SELECT id, text, created_ms FROM dictations ORDER BY id DESC")) {
        while (query.next()) {
            const auto date = QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong());
            const QLocale english(QLocale::English);
            entries.append(QVariantMap{{"id", query.value(0).toString()}, {"text", query.value(1)},
                                       {"date", english.toString(date, "dddd, d MMMM yyyy · HH:mm")}});
        }
    }
    history_ = std::move(entries);
    emit historyChanged();
}

void WhisperModule::copyEntry(const QString &id)
{
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare("SELECT text FROM dictations WHERE id = ?")) {
        query.addBindValue(id);
        if (query.exec() && query.next()) {
            QGuiApplication::clipboard()->setText(query.value(0).toString());
            return;
        }
    }
}

void WhisperModule::deleteEntry(const QString &id)
{
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare("DELETE FROM dictations WHERE id = ?")) {
        query.addBindValue(id);
        if (query.exec())
            refreshHistory();
    }
}

void WhisperModule::setPhase(Phase phase, const QString &status, bool failed)
{
    phase_ = phase;
    status_ = status;
    failed_ = failed;
    emit changed();
}

void WhisperModule::complete(const QString &status, bool failed)
{
    stopTimeout_.stop();
    temporary_.reset();
    setPhase(Phase::Idle, status, failed);
}
