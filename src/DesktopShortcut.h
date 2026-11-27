#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <memory>

class QQuickWindow;

class DesktopShortcut : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.riccify.Clipify")
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString shortcut READ shortcut NOTIFY shortcutChanged)
    Q_PROPERTY(bool canChangeShortcut READ canChangeShortcut CONSTANT)

public:
    enum class Action { History, Screenshot, FullScreen, FocusedWindow, Ocr };
    explicit DesktopShortcut(Action action = Action::History, QObject *parent = nullptr);
    ~DesktopShortcut() override;

    QString error() const { return error_; }
    QString shortcut() const { return shortcut_; }
    bool canChangeShortcut() const;
    Q_INVOKABLE QString recordShortcut(int key, int modifiers);
    Q_INVOKABLE void showAtPointer(QQuickWindow *window);
    Q_INVOKABLE void pasteClipboard();
    Q_SCRIPTABLE Q_INVOKABLE void ShowHistory();
    Q_SCRIPTABLE Q_INVOKABLE void CaptureScreen();

signals:
    void activated();
    void errorChanged();
    void shortcutChanged();

private:
    void setError(const QString &error);
    bool applyShortcut(const QString &accelerator);
    struct Platform;
    std::unique_ptr<Platform> platform_;
    QString error_;
    QString shortcut_;
    Action action_;
    QSettings settings_;
};
