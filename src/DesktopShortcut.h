#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
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
    enum class Action { History, PauseHistory, PauseDictation, Screenshot, FullScreen, FocusedWindow, Ocr, Notes, Whisper, Command };
    explicit DesktopShortcut(Action action = Action::History, QObject *parent = nullptr);
    ~DesktopShortcut() override;
    static QVariantMap defaultAccelerators();
    static QString acceleratorForKey(int key, int modifiers);
    static QString displayAccelerator(const QString &accelerator);
    bool bindCommand(const QString &accelerator);
    bool rememberExternalFocus();

    QString error() const { return error_; }
    QString shortcut() const { return shortcut_; }
    bool canChangeShortcut() const;
    Q_INVOKABLE QString recordShortcut(int key, int modifiers);
    Q_INVOKABLE void showAtPointer(QQuickWindow *window);
    Q_INVOKABLE void pasteClipboard();
    Q_SCRIPTABLE Q_INVOKABLE void ShowHistory();
    Q_SCRIPTABLE Q_INVOKABLE void ToggleHistoryPause();
    Q_SCRIPTABLE Q_INVOKABLE void ToggleDictationHistoryPause();
    Q_SCRIPTABLE Q_INVOKABLE void CaptureScreen();
    Q_SCRIPTABLE Q_INVOKABLE void OpenNotes();
    Q_SCRIPTABLE Q_INVOKABLE void ToggleDictation();

signals:
    void activated();
    void pressed();
    void released();
    void errorChanged();
    void shortcutChanged();

private:
    void releaseForReset();
    void reloadShortcut(bool factoryReset = false);
    void activateFromDesktop();
    void setError(const QString &error);
    bool applyShortcut(const QString &accelerator);
    struct Platform;
    std::unique_ptr<Platform> platform_;
    QString error_;
    QString shortcut_;
    QString activeAccelerator_;
    Action action_;
};
