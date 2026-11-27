#include "CommandShortcuts.h"
#include "AppConfig.h"
#include "DesktopShortcut.h"

#include <QCryptographicHash>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

namespace {
constexpr auto configKey = "commands/entries";
const QRegularExpression validAccelerator(R"(^(?:(?:<Control>|<Alt>|<Super>|<Shift>)*[A-Za-z0-9_]+)?$)");
}

CommandShortcuts::CommandShortcuts(QObject *parent) : QObject(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, [this] { reload(true); });
    connect(&AppConfig::instance(), &AppConfig::activatingConfig, this, [this] { hotkeys_.clear(); });
    reload(false);
}

CommandShortcuts::~CommandShortcuts() = default;

bool CommandShortcuts::hotkeysAvailable() const
{
    return QGuiApplication::platformName() == "xcb";
}

QString CommandShortcuts::keyAccelerator(int key, int modifiers) const
{
    return hotkeysAvailable() ? DesktopShortcut::acceleratorForKey(key, modifiers) : QString();
}

QString CommandShortcuts::shortcutLabel(const QString &accelerator) const
{
    return DesktopShortcut::displayAccelerator(accelerator);
}

QString CommandShortcuts::approvalKey() const
{
    const auto path = AppConfig::instance().activePath().toUtf8();
    return QStringLiteral("commandApprovals/") + QCryptographicHash::hash(path, QCryptographicHash::Sha256).toHex();
}

QByteArray CommandShortcuts::approval(const Entry &entry) const
{
    const auto payload = QJsonDocument(QJsonArray{entry.id, entry.command, entry.accelerator}).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
}

bool CommandShortcuts::isEnabled(const Entry &entry) const
{
    return entry.enabled && approvals_.contains(approval(entry));
}

void CommandShortcuts::setApproval(const Entry &entry, bool approved)
{
    const auto key = approval(entry);
    if (approved)
        approvals_.insert(key);
    else
        approvals_.remove(key);
    QStringList list;
    for (const auto &item : approvals_)
        list.append(QString::fromLatin1(item));
    state_.setValue(approvalKey(), list);
    state_.sync();
}

QVariantList CommandShortcuts::entries() const
{
    QVariantList rows;
    for (const auto &entry : entries_) {
        rows.append(QVariantMap{{"id", entry.id}, {"name", entry.name}, {"command", entry.command},
                                {"accelerator", entry.accelerator}, {"shortcut", shortcutLabel(entry.accelerator)},
                                {"enabled", isEnabled(entry)}, {"running", running_.contains(entry.id)},
                                {"error", hotkeyErrors_.value(entry.id)}, {"result", results_.value(entry.id)}});
    }
    return rows;
}

void CommandShortcuts::reload(bool imported)
{
    hotkeys_.clear();
    hotkeyErrors_.clear();
    approvals_.clear();
    results_.clear();
    if (imported) {
        state_.remove(approvalKey());
        state_.sync();
    } else {
        for (const auto &item : state_.value(approvalKey()).toStringList())
            approvals_.insert(item.toLatin1());
    }

    entries_.clear();
    QSet<QString> ids;
    for (const auto &row : AppConfig::instance().value(configKey).toList()) {
        const auto map = row.toMap();
        const Entry entry{map.value("id").toString(), map.value("name").toString(), map.value("command").toString(),
                          map.value("accelerator").toString(), map.value("enabled").toBool()};
        if (entry.id.isEmpty() || ids.contains(entry.id) || entry.name.trimmed().isEmpty()
            || entry.command.trimmed().isEmpty() || !validAccelerator.match(entry.accelerator).hasMatch())
            continue;
        ids.insert(entry.id);
        entries_.append(entry);
        if (isEnabled(entry))
            hotkeyErrors_.insert(entry.id, registerHotkey(entry));
    }
    setMessage({});
}

bool CommandShortcuts::persist(const Entries &entries)
{
    QVariantList rows;
    for (const auto &entry : entries) {
        rows.append(QVariantMap{{"id", entry.id}, {"name", entry.name}, {"command", entry.command},
                                {"accelerator", entry.accelerator}, {"enabled", entry.enabled}});
    }
    return AppConfig::instance().setValue(configKey, rows);
}

QString CommandShortcuts::save(const QString &id, const QString &name, const QString &command,
                               const QString &accelerator)
{
    if (name.trimmed().isEmpty() || command.trimmed().isEmpty())
        return tr("Enter a name and a command.");
    if (!validAccelerator.match(accelerator).hasMatch() || accelerator == "<Control>v")
        return tr("Choose a different shortcut.");

    auto updated = entries_;
    auto old = std::find_if(updated.begin(), updated.end(), [&id](const Entry &entry) { return entry.id == id; });
    if (!id.isEmpty() && old == updated.end())
        return tr("This command is no longer available.");

    const bool creating = old == updated.end();
    const Entry previous = creating ? Entry{} : *old;
    const Entry entry{creating ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id,
                      name.trimmed(), command, accelerator, creating || isEnabled(previous)};
    if (creating)
        updated.append(entry);
    else
        *old = entry;
    if (!persist(updated))
        return tr("Could not save the command.");

    hotkeys_.erase(entry.id);
    if (!creating)
        setApproval(previous, false);
    setApproval(entry, entry.enabled);
    entries_ = std::move(updated);
    results_.remove(entry.id);
    hotkeyErrors_.insert(entry.id, entry.enabled ? registerHotkey(entry) : QString());
    setMessage({});
    return {};
}

void CommandShortcuts::setEnabled(const QString &id, bool enabled)
{
    auto updated = entries_;
    for (auto &entry : updated) {
        if (entry.id != id)
            continue;
        entry.enabled = enabled;
        if (!persist(updated)) {
            setMessage(tr("Could not save the command."));
            return;
        }
        const Entry saved = entry;
        entries_ = std::move(updated);
        hotkeys_.erase(id);
        setApproval(saved, enabled);
        hotkeyErrors_.insert(id, enabled ? registerHotkey(saved) : QString());
        setMessage({});
        return;
    }
}

void CommandShortcuts::remove(const QString &id)
{
    auto updated = entries_;
    const auto entry = std::find_if(updated.begin(), updated.end(), [&id](const Entry &candidate) { return candidate.id == id; });
    if (entry == updated.end())
        return;

    const Entry removed = *entry;
    updated.erase(entry);
    if (!persist(updated)) {
        setMessage(tr("Could not remove the command."));
        return;
    }
    setApproval(removed, false);
    entries_ = std::move(updated);
    hotkeys_.erase(id);
    hotkeyErrors_.remove(id);
    results_.remove(id);
    setMessage({});
}

QString CommandShortcuts::registerHotkey(const Entry &entry)
{
    if (entry.accelerator.isEmpty())
        return {};
    if (!hotkeysAvailable())
        return tr("Global shortcuts are unavailable on this desktop.");

    auto shortcut = std::make_unique<DesktopShortcut>(DesktopShortcut::Action::Command);
    if (!shortcut->bindCommand(entry.accelerator))
        return tr("Shortcut unavailable. Choose a different combination.");
    connect(shortcut.get(), &DesktopShortcut::activated, this, [this, id = entry.id] { run(id); });
    hotkeys_.emplace(entry.id, std::move(shortcut));
    return {};
}

void CommandShortcuts::run(const QString &id)
{
    const auto entry = std::find_if(entries_.cbegin(), entries_.cend(), [&id](const Entry &candidate) { return candidate.id == id; });
    if (entry == entries_.cend() || !isEnabled(*entry) || running_.contains(id))
        return;

    auto *process = new QProcess(this);
    running_.insert(id, process);
    results_.remove(id);
    process->setWorkingDirectory(QDir::homePath());
    process->setStandardOutputFile(QProcess::nullDevice());
    connect(process, &QProcess::readyReadStandardError, this, [process] {
        const auto data = process->property("stderrTail").toByteArray() + process->readAllStandardError();
        process->setProperty("stderrTail", data.right(2048));
    });
    connect(process, &QProcess::errorOccurred, this, [this, id, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(id, process, tr("Could not start the command: %1").arg(process->errorString()));
    });
    connect(process, &QProcess::finished, this, [this, id, process](int code, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && code == 0) {
            finish(id, process, {});
            return;
        }
        const auto stderrTail = process->property("stderrTail").toByteArray() + process->readAllStandardError();
        const auto details = QString::fromLocal8Bit(stderrTail.right(2048)).trimmed();
        const QString failure = status == QProcess::CrashExit ? tr("Command stopped unexpectedly.")
                                                              : tr("Command exited with code %1.").arg(code);
        finish(id, process, details.isEmpty() ? failure : failure + ' ' + details);
    });

#ifdef Q_OS_WIN
    process->start("cmd.exe", {"/D", "/C", entry->command});
#else
    process->start("/bin/sh", {"-c", entry->command});
#endif
    process->closeWriteChannel();
    emit changed();
}

void CommandShortcuts::finish(const QString &id, QProcess *process, const QString &error)
{
    running_.remove(id);
    results_.insert(id, error.isEmpty() ? tr("Command completed.") : error);
    process->deleteLater();
    emit changed();
}

void CommandShortcuts::setMessage(const QString &message)
{
    message_ = message;
    emit changed();
}
