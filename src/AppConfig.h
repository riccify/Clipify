#pragma once

#include <QObject>
#include <QVariantMap>

class AppConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoStart READ autoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageError READ messageError NOTIFY messageChanged)
    Q_PROPERTY(QString activePath READ activePath NOTIFY activePathChanged)

public:
    static AppConfig &instance();
    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    bool setValue(const QString &key, const QVariant &value);
    bool autoStart() const;
    QString message() const { return message_; }
    bool messageError() const { return messageError_; }
    QString activePath() const { return path_; }

    Q_INVOKABLE void setAutoStart(bool enabled);
    Q_INVOKABLE void loadConfig();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void openConfigFolder();
    Q_INVOKABLE void resetToDefaults();
    void reportShortcutConflict(const QString &shortcut);

signals:
    void activatingConfig();
    void resetConfirmationRequested();
    void restoringDefaults();
    void reloaded(bool factoryReset);
    void autoStartChanged();
    void messageChanged();
    void activePathChanged();

private:
    AppConfig();
    bool store(const QVariantMap &values, const QString &path);
    void activateConfig(const QVariantMap &values, const QString &path, bool factoryReset);
    void setMessage(const QString &message, bool error = true);

    QVariantMap values_;
    QString path_;
    QString message_;
    bool messageError_ = false;
    QStringList shortcutConflicts_;
    bool unreadable_ = false;
};
