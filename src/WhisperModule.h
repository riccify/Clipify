#pragma once

#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantList>
#include <memory>

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
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    explicit WhisperModule(QObject *parent = nullptr);
    bool recording() const { return phase_ == Phase::Recording; }
    bool busy() const { return phase_ != Phase::Idle; }
    QString status() const { return status_; }
    bool failed() const { return failed_; }
    QString modelPath() const;
    QString language() const;
    bool holdToTalk() const;
    QVariantList history() const { return history_; }

    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void placeOverlay(QQuickWindow *window) const;
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
    void transcriptReady();

private:
    enum class Phase { Idle, Recording, Finishing, Transcribing, Cancelling };
    void setPhase(Phase phase, const QString &status, bool failed = false);
    void startRecording();
    void stopRecorder();
    void transcribe();
    void saveTranscript(const QString &text);
    void refreshHistory();
    void complete(const QString &status, bool failed = false);

    QSqlDatabase database_;
    QVariantList history_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QProcess recorder_;
    QProcess whisper_;
    QTimer stopTimeout_;
    Phase phase_ = Phase::Idle;
    QString status_;
    QString jobModel_;
    QString jobLanguage_;
    QString executable_;
    bool failed_ = false;
    bool stopTimedOut_ = false;
};
