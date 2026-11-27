#include "AppConfig.h"
#include "DesktopShortcut.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr auto autoStartKey = "general/autoStart";
constexpr auto activePathKey = "config/activePath";

QDir configFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

QVariantMap defaults()
{
    return {{"clipboard/historyNumber", 20},
            {"clipboard/historyLimit", 0},
            {"clipboard/skipSensitive", true},
            {"clipboard/accurateOcr", false},
            {"screen/saveFolder", QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
                                      .filePath("Screenshots")},
            {"notes/defaultFolder", QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                        .filePath("Clipify Notes")},
            {autoStartKey, false},
            {"commands/entries", QVariantList{}},
            {"converter/targetFormat", QStringLiteral("png")},
            {"whisper/historyLimit", 0},
            {"whisper/modelPath", QStringLiteral("")},
            {"whisper/language", QStringLiteral("auto")},
            {"whisper/holdToTalk", false},
            {"whisper/polishText", false},
            {"llm/cloud", false},
            {"llm/provider", QStringLiteral("openrouter")},
            {"llm/polishModel/openrouter", QStringLiteral("")},
            {"llm/polishModel/openai", QStringLiteral("")},
            {"llm/polishModel/anthropic", QStringLiteral("")}};
}

QVariantMap factoryDefaults()
{
    auto settings = defaults();
    settings.insert(DesktopShortcut::defaultAccelerators());
    return settings;
}

QString defaultConfigPath()
{
    return configFolder().filePath("default-config.json");
}

bool writeJson(const QString &path, const QVariantMap &values)
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument::fromVariant(values).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

QString readJson(const QString &path, QVariantMap &values)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QObject::tr("Could not read the file.");

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        return QObject::tr("JSON syntax error.");
    const auto invalidSettings = QObject::tr("Invalid Clipify settings.");
    if (!document.isObject())
        return invalidSettings;

    const auto object = document.object();
    bool recognized = false;
    const auto expected = defaults();
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        if (!object.contains(it.key()))
            continue;
        recognized = true;
        const auto entry = object.value(it.key());
        if (it.value().typeId() == QMetaType::Bool && !entry.isBool())
            return invalidSettings;
        if (it.value().typeId() == QMetaType::Int && entry.toInt(-1) < (it.value().toInt() == 0 ? 0 : 1))
            return invalidSettings;
        if (it.value().typeId() == QMetaType::QVariantList && !entry.isArray())
            return invalidSettings;
        if (it.value().typeId() == QMetaType::QString) {
            if (!entry.isString())
                return invalidSettings;
            const QString value = entry.toString();
            if (it.key() == "whisper/language") {
                if (value != "auto" && value != "de" && value != "en")
                    return invalidSettings;
            } else if (it.key() == "converter/targetFormat") {
                static const QStringList formats{"png", "jpg", "webp", "bmp", "tiff"};
                if (!formats.contains(value))
                    return invalidSettings;
            } else if (it.key() == "llm/provider") {
                if (value != "openrouter" && value != "openai" && value != "anthropic")
                    return invalidSettings;
            } else if (!it.key().startsWith("llm/polishModel/")
                       && !(it.key() == "whisper/modelPath" && value.isEmpty())
                       && !QDir::isAbsolutePath(value)) {
                return invalidSettings;
            }
        }
    }

    static const QRegularExpression validShortcut(
        R"(^(?:(?:<Control>|<Alt>|<Super>|<Shift>)*[A-Za-z0-9_]+)?$)");
    for (const auto &key : DesktopShortcut::defaultAccelerators().keys()) {
        if (!object.contains(key))
            continue;
        recognized = true;
        const auto entry = object.value(key);
        if (!entry.isString() || !validShortcut.match(entry.toString()).hasMatch()
            || entry.toString() == "<Control>v")
            return invalidSettings;
    }
    if (!recognized)
        return invalidSettings;

    values = object.toVariantMap();
    return {};
}

#ifdef Q_OS_WIN
QSettings startupRegistry()
{
    return QSettings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)", QSettings::NativeFormat);
}
#else
QString autostartPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath("autostart/io.github.riccify.Clipify-autostart.desktop");
}
#endif

bool registeredAutostart()
{
#ifdef Q_OS_WIN
    return startupRegistry().contains("Clipify");
#elif defined(Q_OS_LINUX)
    QFile entry(autostartPath());
    if (!entry.open(QIODevice::ReadOnly))
        return false;
    const auto content = entry.readAll();
    return !content.contains("Hidden=true") && !content.contains("X-GNOME-Autostart-enabled=false");
#else
    return false;
#endif
}

bool updateAutostart(bool enabled)
{
#ifdef Q_OS_WIN
    auto registry = startupRegistry();
    if (enabled)
        registry.setValue("Clipify", '"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" --background");
    else
        registry.remove("Clipify");
    registry.sync();
    return registry.status() == QSettings::NoError;
#elif defined(Q_OS_LINUX)
    const QString path = autostartPath();
    if (!enabled)
        return !QFileInfo::exists(path) || QFile::remove(path);

    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QString executable = QCoreApplication::applicationFilePath();
    executable.replace("\\", "\\\\\\\\").replace("\"", "\\\\\"")
              .replace("$", "\\\\$").replace("`", "\\\\`").replace("%", "%%");
    const QByteArray data = "[Desktop Entry]\nType=Application\nName=Clipify\n"
        "Icon=io.github.riccify.Clipify\nTerminal=false\nExec=\"" + executable.toUtf8() + "\" --background\n";
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
#else
    return !enabled;
#endif
}
}

AppConfig &AppConfig::instance()
{
    static AppConfig config;
    return config;
}

AppConfig::AppConfig()
    : values_(defaults()), path_(configFolder().filePath("config.json"))
{
    QVariantMap templateValues;
    const auto factory = factoryDefaults();
    if (!readJson(defaultConfigPath(), templateValues).isEmpty() || templateValues != factory) {
        if (!QDir().mkpath(configFolder().absolutePath()) || !writeJson(defaultConfigPath(), factory))
            setMessage(tr("Could not create the default config file."));
    }

    QSettings legacy;
    const QString selectedPath = legacy.value(activePathKey).toString();
    if (!selectedPath.isEmpty())
        path_ = selectedPath;

    if (!selectedPath.isEmpty() || QFileInfo::exists(path_)) {
        QVariantMap loaded;
        const QString error = readJson(path_, loaded);
        if (error.isEmpty()) {
            values_.insert(loaded);
            if (autoStart() != registeredAutostart() && !updateAutostart(autoStart()))
                setMessage(tr("Could not apply the saved autostart setting."));
        } else {
            unreadable_ = true;
            setMessage(tr("Config not loaded. %1").arg(error));
        }
        return;
    }

    const auto keys = values_.keys() + DesktopShortcut::defaultAccelerators().keys();
    for (const auto &key : keys) {
        if (!legacy.contains(key))
            continue;
        const QVariant fallback = values_.value(key);
        QVariant value = legacy.value(key);
        if (fallback.isValid() && !value.convert(fallback.metaType()))
            continue;
        if (fallback.typeId() == QMetaType::Int && value.toInt() < 1)
            continue;
        if ((key == "screen/saveFolder" || key == "notes/defaultFolder")
            && !QDir::isAbsolutePath(value.toString()))
            continue;
        values_.insert(key, value);
    }
    values_.insert(autoStartKey, registeredAutostart());
    store(values_, path_);
}

QVariant AppConfig::value(const QString &key, const QVariant &fallback) const
{
    return values_.value(key, fallback);
}

bool AppConfig::store(const QVariantMap &values, const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !writeJson(path, values)) {
        setMessage(tr("Could not write the config file."));
        return false;
    }
    values_ = values;
    return true;
}

bool AppConfig::setValue(const QString &key, const QVariant &value)
{
    return setValues({{key, value}});
}

bool AppConfig::setValues(const QVariantMap &values)
{
    if (unreadable_) {
        setMessage(tr("Load a valid config before changing settings."));
        return false;
    }
    auto updated = values_;
    updated.insert(values);
    return updated == values_ || store(updated, path_);
}

bool AppConfig::autoStart() const
{
    return value(autoStartKey).toBool();
}

void AppConfig::setAutoStart(bool enabled)
{
    const bool previous = autoStart();
    if (!updateAutostart(enabled)) {
        setMessage(tr("Could not change autostart."));
    } else if (!setValue(autoStartKey, enabled)) {
        updateAutostart(previous);
    } else {
        setMessage({});
    }
    emit autoStartChanged();
}

void AppConfig::loadConfig()
{
    const QString selected = QFileDialog::getOpenFileName(nullptr, tr("Load config"),
                                  QFileInfo(path_).absolutePath(), tr("JSON files (*.json)"));
    const QString path = QFileInfo(selected).canonicalFilePath();
    if (selected.isEmpty())
        return;
    if (path.isEmpty())
        return setMessage(tr("Config not loaded. Could not read the file."));
    if (path == QFileInfo(defaultConfigPath()).canonicalFilePath()) {
        emit resetConfirmationRequested();
        return;
    }

    QVariantMap imported;
    const QString error = readJson(path, imported);
    if (!error.isEmpty()) {
        setMessage(tr("Config not loaded. %1").arg(error));
        return;
    }

    auto updated = values_;
    updated.insert(imported);
    activateConfig(updated, path, false);
}

void AppConfig::resetToDefaults()
{
    const QString localPath = configFolder().filePath("config.json");
    if (QFileInfo(localPath).isSymLink()) {
        setMessage(tr("Reset failed. The local config is a symbolic link."));
        return;
    }
    activateConfig(factoryDefaults(), localPath, true);
}

void AppConfig::activateConfig(const QVariantMap &updated, const QString &path, bool factoryReset)
{
    const bool previous = registeredAutostart();
    const bool enabled = updated.value(autoStartKey).toBool();
    if (enabled != previous && !updateAutostart(enabled)) {
        setMessage(factoryReset ? tr("Reset failed. Could not turn off autostart.")
                                : tr("Config not loaded. Could not apply the autostart setting."));
        return;
    }
    if (!store(updated, path)) {
        if (enabled != previous)
            updateAutostart(previous);
        setMessage(factoryReset ? tr("Reset failed. Could not write the local config.")
                                : tr("Config not loaded. Could not write to the selected file."));
        return;
    }

    unreadable_ = false;
    if (path_ != path) {
        path_ = path;
        QSettings state;
        state.setValue(activePathKey, path_);
        state.sync();
        emit activePathChanged();
    }
    shortcutConflicts_.clear();
    emit activatingConfig();
    if (factoryReset)
        emit restoringDefaults();
    emit reloaded(factoryReset);
    emit autoStartChanged();
    QString message = factoryReset ? tr("Factory settings restored.") : tr("Config loaded.");
    if (!shortcutConflicts_.isEmpty())
        message += tr(" Unavailable shortcuts: %1.").arg(shortcutConflicts_.join(", "));
    setMessage(message, false);
}

void AppConfig::saveConfig()
{
    QFileDialog dialog(nullptr, tr("Save config"), QDir::home().filePath("config.json"), tr("JSON files (*.json)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("json");
    if (dialog.exec() == QDialog::Accepted) {
        const QString path = dialog.selectedFiles().first();
        const QFileInfo target(path);
        if (target.absoluteFilePath() == defaultConfigPath()
            || (!target.canonicalFilePath().isEmpty()
                && target.canonicalFilePath() == QFileInfo(defaultConfigPath()).canonicalFilePath())) {
            setMessage(tr("Choose a different name to keep the default config available."));
            return;
        }
        const bool saved = writeJson(path, values_);
        setMessage(saved ? tr("Config saved.") : tr("Config not saved. Could not write the file."), !saved);
    }
}

void AppConfig::openConfigFolder()
{
    const QDir folder = QFileInfo(path_).absoluteDir();
    if (!folder.exists() || !QDesktopServices::openUrl(QUrl::fromLocalFile(folder.absolutePath())))
        setMessage(tr("Could not open the config folder."));
}

void AppConfig::reportShortcutConflict(const QString &shortcut)
{
    shortcutConflicts_.append(shortcut);
}

void AppConfig::setMessage(const QString &message, bool error)
{
    message_ = message;
    messageError_ = error && !message.isEmpty();
    emit messageChanged();
}
