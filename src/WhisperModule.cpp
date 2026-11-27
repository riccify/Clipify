#include "WhisperModule.h"
#include "AppConfig.h"
#include "LlmService.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QNetworkReply>
#include <QQuickWindow>
#include <QSaveFile>
#include <QScreen>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QtEndian>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_LINUX
#include <csignal>
#include <unistd.h>
#endif

namespace {
constexpr auto modelKey = "whisper/modelPath";
constexpr auto languageKey = "whisper/language";
constexpr auto holdKey = "whisper/holdToTalk";
constexpr auto historyLimitKey = "whisper/historyLimit";
constexpr int waveformBars = 12;
constexpr int samplesPerBar = 80;

qint64 wavDataOffset(QFile &file)
{
    const QByteArray header = file.read(12);
    if (header.size() != 12 || !header.startsWith("RIFF") || header.mid(8) != "WAVE")
        return -1;

    while (file.pos() + 8 <= file.size()) {
        const QByteArray chunk = file.read(8);
        if (chunk.size() != 8)
            break;
        if (chunk.startsWith("data"))
            return file.pos();
        const qint64 size = qFromLittleEndian<quint32>(chunk.constData() + 4);
        if (!file.seek(file.pos() + size + (size & 1)))
            break;
    }
    return -1;
}

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

constexpr auto promptName = "dictation-polishing.md";

QString userPromptPath()
{
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    return data.filePath(QStringLiteral("prompts/") + promptName);
}

QString polishingPromptPath()
{
    const auto userPath = userPromptPath();
    if (QFileInfo::exists(userPath))
        return userPath;

    const QDir application(QCoreApplication::applicationDirPath());
    const auto adjacent = application.filePath(QStringLiteral("prompts/") + promptName);
#ifdef CLIPIFY_INSTALLED_PROMPT_DIR
    if (!QFileInfo::exists(adjacent))
        return application.filePath(QStringLiteral(CLIPIFY_INSTALLED_PROMPT_DIR "/") + promptName);
#endif
    return adjacent;
}

QString processDetail(QProcess &process)
{
    return QString::fromLocal8Bit(process.readAllStandardError().right(1200)).trimmed();
}
}

WhisperModule::WhisperModule(LlmService &llm, QObject *parent) : QObject(parent), llm_(llm)
{
    waveform_.fill(0.0, waveformBars);
    waveformTimer_.setInterval(60);
    connect(&waveformTimer_, &QTimer::timeout, this, &WhisperModule::updateWaveform);
    connect(&historyPause_, &HistoryPause::changed, this, &WhisperModule::pausedChanged);
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, [this] {
        enforceHistoryLimit();
        emit settingsChanged();
    });
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    database_ = QSqlDatabase::addDatabase("QSQLITE", "whisper-history");
    database_.setDatabaseName(data.filePath("dictation.sqlite3"));
    if (QDir().mkpath(data.absolutePath()) && database_.open()) {
        QSqlQuery schema(database_);
        schema.exec("PRAGMA secure_delete = ON");
        if (!schema.exec("CREATE TABLE IF NOT EXISTS dictations "
                         "(id INTEGER PRIMARY KEY, text TEXT NOT NULL, created_ms INTEGER NOT NULL)"))
            database_.close();
    }
    enforceHistoryLimit();

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
        temporary_.reset();
        processTranscript(transcript);
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

QString WhisperModule::openPolishingPrompt() const
{
    QString path = polishingPromptPath();
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable())
        return tr("Could not read the dictation prompt: %1").arg(info.absoluteFilePath());

    if (!info.isWritable() || !QFileInfo(info.absolutePath()).isWritable()) {
        QFile source(path);
        path = userPromptPath();
        if (!source.open(QIODevice::ReadOnly) || !QDir().mkpath(QFileInfo(path).absolutePath()))
            return tr("Could not create an editable copy of the dictation prompt.");
        const QByteArray contents = source.readAll();
        QSaveFile editable(path);
        if (source.error() != QFileDevice::NoError || !editable.open(QIODevice::WriteOnly)
            || editable.write(contents) != contents.size() || !editable.commit())
            return tr("Could not create an editable copy of the dictation prompt.");
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        return tr("Could not open the dictation prompt: %1").arg(path);
    return {};
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
    if (polishReply_)
        polishReply_->abort();
    else if (recorder_.state() != QProcess::NotRunning)
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
                        area.top());
}

void WhisperModule::updateWaveform()
{
    if (!temporary_)
        return;

    QFile file(temporary_->filePath("recording.wav"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const qint64 offset = wavDataOffset(file);
    if (offset < 0)
        return;

    const qint64 end = offset + ((file.size() - offset) / 2) * 2;
    if (end <= waveformPosition_) {
        bool changed = false;
        for (auto &level : waveform_) {
            const double previous = level.toDouble();
            if (previous > 0) {
                level = previous > 0.04 ? previous * 0.7 : 0.0;
                changed = true;
            }
        }
        if (changed)
            emit waveformChanged();
        return;
    }

    const qint64 start = std::max(offset, end - 2 * waveformBars * samplesPerBar);
    if (!file.seek(start))
        return;
    const QByteArray samples = file.read(end - start);
    const auto available = samples.size() / 2;
    if (available < waveformBars)
        return;
    waveformPosition_ = end;

    for (int bar = 0; bar < waveformBars; ++bar) {
        const auto begin = bar * available / waveformBars;
        const auto finish = (bar + 1) * available / waveformBars;
        double sumSquares = 0.0;
        for (auto sample = begin; sample < finish; ++sample) {
            const double amplitude = qFromLittleEndian<qint16>(samples.constData() + 2 * sample) / 32768.0;
            sumSquares += amplitude * amplitude;
        }
        const double rms = std::sqrt(sumSquares / (finish - begin));
        const double level = std::clamp((rms - 0.004) * 8.0, 0.0, 1.0);
        waveform_[bar] = std::pow(level, 0.75);
    }
    emit waveformChanged();
}

int WhisperModule::historyLimit() const
{
    return AppConfig::instance().value(historyLimitKey, 0).toInt();
}

void WhisperModule::setHistoryLimit(int limit)
{
    if (limit >= 0 && historyLimit() != limit && AppConfig::instance().setValue(historyLimitKey, limit)) {
        enforceHistoryLimit();
        emit settingsChanged();
    }
}

bool WhisperModule::pruneHistory()
{
    const int limit = historyLimit();
    if (limit == 0)
        return true;

    QSqlQuery query(database_);
    if (!query.prepare("DELETE FROM dictations WHERE id < "
                       "(SELECT id FROM dictations ORDER BY id DESC LIMIT 1 OFFSET ?)"))
        return false;

    query.addBindValue(limit - 1);
    return query.exec();
}

void WhisperModule::enforceHistoryLimit()
{
    if (!database_.isOpen())
        return;
    if (pruneHistory()) {
        refreshHistory();
    } else if (!busy()) {
        complete(tr("Could not apply the dictation history limit."), true);
    }
}

void WhisperModule::processTranscript(const QString &text)
{
    const QString model = llm_.polishModel();
    if (model.isEmpty() || !llm_.cloud()) {
        saveTranscript(text);
        return;
    }

    QFile prompt(polishingPromptPath());
    if (!prompt.open(QIODevice::ReadOnly)) {
        saveTranscript(text, tr("Could not read the dictation prompt."));
        return;
    }
    const QString instructions = QString::fromUtf8(prompt.readAll()).trimmed();
    if (prompt.error() != QFileDevice::NoError || instructions.isEmpty()) {
        saveTranscript(text, tr("The dictation prompt is empty or unreadable."));
        return;
    }

    polishReply_ = llm_.generateText(model, instructions, text, this,
        [this, text](const QString &polished, const QString &error) {
            polishReply_ = nullptr;
            if (phase_ == Phase::Cancelling) {
                complete(tr("Dictation cancelled."));
                return;
            }
            saveTranscript(error.isEmpty() ? polished : text, error);
        });

    if (polishReply_)
        setPhase(Phase::Polishing, tr("Polishing dictation with AI…"));
    else
        saveTranscript(text, tr("Configure a cloud provider, model and API key in LLM Settings."));
}

void WhisperModule::saveTranscript(const QString &text, const QString &polishError)
{
    const auto copied = [this, &polishError](const QString &status) {
        if (polishError.isEmpty())
            complete(status);
        else
            complete(tr("%1 Original text used. %2").arg(status, polishError), true);
    };
    QGuiApplication::clipboard()->setText(text);
    emit transcriptReady();
    if (paused()) {
        copied(tr("Dictation copied. History is paused."));
        return;
    }
    if (database_.isOpen() && database_.transaction()) {
        QSqlQuery query(database_);
        if (query.prepare("INSERT INTO dictations (text, created_ms) VALUES (?, ?)")) {
            query.addBindValue(text);
            query.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (query.exec()) {
                query.finish();
                if (pruneHistory() && database_.commit()) {
                    refreshHistory();
                    copied(tr("Dictation copied."));
                    return;
                }
            }
        }
        database_.rollback();
    }
    const QString historyError = tr("Dictation copied, but it could not be saved to history.");
    complete(polishError.isEmpty() ? historyError
                                  : tr("%1 Original text used. %2").arg(historyError, polishError), true);
}

void WhisperModule::refreshHistory()
{
    QVariantList entries;
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare("SELECT id, text, created_ms FROM dictations "
              "WHERE instr(lower(text), lower(?)) > 0 ORDER BY id DESC")) {
        query.addBindValue(historySearch_.isEmpty() ? QStringLiteral("") : historySearch_);
        if (!query.exec())
            return;
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

void WhisperModule::setHistorySearch(const QString &query)
{
    const QString search = query.trimmed();
    if (search == historySearch_)
        return;
    historySearch_ = search;
    refreshHistory();
}

void WhisperModule::clearHistory()
{
    QSqlQuery query(database_);
    if (database_.isOpen() && query.exec("DELETE FROM dictations"))
        refreshHistory();
    else if (!busy())
        complete(tr("Could not clear the dictation history."), true);
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
    if (phase == Phase::Recording) {
        waveformPosition_ = 0;
        waveform_.fill(0.0, waveformBars);
        emit waveformChanged();
        waveformTimer_.start();
    } else {
        waveformTimer_.stop();
    }
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
