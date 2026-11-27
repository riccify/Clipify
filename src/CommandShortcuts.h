#pragma once

#include <QObject>
#include <QSet>
#include <QSettings>
#include <QVariantList>
#include <map>
#include <memory>

class DesktopShortcut;
class QProcess;

class CommandShortcuts : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(bool hotkeysAvailable READ hotkeysAvailable CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    explicit CommandShortcuts(QObject *parent = nullptr);
    ~CommandShortcuts() override;
    QVariantList entries() const;
    bool hotkeysAvailable() const;
    QString message() const { return message_; }

    Q_INVOKABLE QString keyAccelerator(int key, int modifiers) const;
    Q_INVOKABLE QString shortcutLabel(const QString &accelerator) const;
    Q_INVOKABLE QString save(const QString &id, const QString &name, const QString &command,
                            const QString &accelerator);
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void run(const QString &id);

signals:
    void changed();

private:
    struct Entry {
        QString id;
        QString name;
        QString command;
        QString accelerator;
        bool enabled = false;
    };

    using Entries = QList<Entry>;
    void reload(bool imported);
    bool persist(const Entries &entries);
    void setApproval(const Entry &entry, bool approved);
    QByteArray approval(const Entry &entry) const;
    QString approvalKey() const;
    bool isEnabled(const Entry &entry) const;
    QString registerHotkey(const Entry &entry);
    void finish(const QString &id, QProcess *process, const QString &error);
    void setMessage(const QString &message);

    Entries entries_;
    QSet<QByteArray> approvals_;
    QSettings state_;
    std::map<QString, std::unique_ptr<DesktopShortcut>> hotkeys_;
    QHash<QString, QString> hotkeyErrors_;
    QHash<QString, QProcess *> running_;
    QHash<QString, QString> results_;
    QString message_;
};
