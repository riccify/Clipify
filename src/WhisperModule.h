#pragma once

#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantList>
#include <memory>
#include "HistoryPause.h"

class LlmService;
class QNetworkReply;
class QQuickWindow;

class WhisperModule : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(QString modelPath READ modelPath NOTIFY settingsChanged)
    Q_PROPERTY(QString language READ language NOTIFY settingsChanged)
    Q_PROPERTY(bool holdToTalk READ holdToTalk NOTIFY settingsChanged)
    Q_PROPERTY(int historyLimit READ historyLimit WRITE setHistoryLimit NOTIFY settingsChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList waveform READ waveform NOTIFY waveformChanged)

public:
    explicit WhisperModule(LlmService &llm, QObject *parent = nullptr);
    bool recording() const { return phase_ == Phase::Recording; }
    bool busy() const { return phase_ != Phase::Idle; }
    QString status() const { return status_; }
    bool failed() const { return failed_; }
    QString modelPath() const;
    QString language() const;
    bool holdToTalk() const;
    int historyLimit() const;
    void setHistoryLimit(int limit);
    bool paused() const { return historyPause_.paused(); }
    Q_INVOKABLE void pauseHistory(int seconds) { historyPause_.pause(seconds); }
    Q_INVOKABLE void resumeHistory() { historyPause_.resume(); }
    Q_INVOKABLE void setHistorySearch(const QString &query);
    Q_INVOKABLE void clearHistory();
    QVariantList history() const { return history_; }
    QVariantList waveform() const { return waveform_; }

    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void placeOverlay(QQuickWindow *window) const;
    Q_INVOKABLE QString openPolishingPrompt() const;
    Q_INVOKABLE void chooseModel();
    Q_INVOKABLE void useDefaultModel();
    Q_INVOKABLE void setLanguage(const QString &language);
    Q_INVOKABLE void setHoldToTalk(bool enabled);
    Q_INVOKABLE void copyEntry(const QString &id);
    Q_INVOKABLE void deleteEntry(const QString &id);

signals:
    void changed();
    void settingsChanged();
    void historyChanged();
    void pausedChanged();
    void transcriptReady();
    void waveformChanged();

private:
    enum class Phase { Idle, Recording, Finishing, Transcribing, Polishing, Cancelling };
    void setPhase(Phase phase, const QString &status, bool failed = false);
    void startRecording();
    void stopRecorder();
    void transcribe();
    void updateWaveform();
    void processTranscript(const QString &text);
    void saveTranscript(const QString &text, const QString &polishError = {});
    void refreshHistory();
    void enforceHistoryLimit();
    bool pruneHistory();
    void complete(const QString &status, bool failed = false);

    LlmService &llm_;
    QNetworkReply *polishReply_ = nullptr;
    HistoryPause historyPause_{"whisper/pausedIndefinitely"};
    QString historySearch_;
    QSqlDatabase database_;
    QVariantList history_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QProcess recorder_;
    QProcess whisper_;
    QTimer stopTimeout_;
    QTimer waveformTimer_;
    QVariantList waveform_;
    qint64 waveformPosition_ = 0;
    Phase phase_ = Phase::Idle;
    QString status_;
    QString jobModel_;
    QString jobLanguage_;
    QString executable_;
    bool failed_ = false;
    bool stopTimedOut_ = false;
};
