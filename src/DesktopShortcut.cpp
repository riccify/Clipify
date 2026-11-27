#include "DesktopShortcut.h"
#include "AppConfig.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QLibrary>
#include <QProcess>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QSocketNotifier>
#include <QTimer>
#include <algorithm>

#ifdef CLIPIFY_X11
#include <QDBusConnection>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

struct DesktopShortcut::Platform
{
#ifdef CLIPIFY_X11
    using FakeKeyEvent = int (*)(Display *, unsigned int, int, unsigned long);

    QLibrary xtest{"libXtst.so.6"};
    FakeKeyEvent fakeKeyEvent = nullptr;
    Display *display = nullptr;
    Window previousFocus = None;
    Time shortcutTime = CurrentTime;
    KeyCode vKey = 0;
    KeyCode controlKey = 0;
    KeyCode shortcutKey = 0;
    unsigned int shortcutMask = 0;
    bool usesGnome = false;
    bool ready = false;
    bool keyHeld = false;
    QTimer releasePoll;
    std::unique_ptr<QSocketNotifier> notifier;

    ~Platform()
    {
        notifier.reset();
        if (display)
            XCloseDisplay(display);
    }
#endif
};

namespace {
struct ShortcutDefinition {
    const char *label;
    const char *accelerator;
    const char *settingsKey;
    const char *desktopName;
    const char *dbusSuffix;
};

ShortcutDefinition shortcutDefinition(DesktopShortcut::Action action)
{
    using Action = DesktopShortcut::Action;
    switch (action) {
    case Action::Screenshot:
        return {"Super+Shift+S", "<Super><Shift>s", "screen/shortcutAccelerator", "clipify-screen", "Screen"};
    case Action::FullScreen:
        return {"Ctrl+Print", "<Control>Print", "screen/fullScreenAccelerator", "clipify-full-screen", "FullScreen"};
    case Action::FocusedWindow:
        return {"Super+Print", "<Super>Print", "screen/focusedWindowAccelerator", "clipify-focused-window", "FocusedWindow"};
    case Action::Ocr:
        return {"Alt+Print", "<Alt>Print", "clipboard/ocrAccelerator", "clipify-ocr", "Ocr"};
    case Action::Notes:
        return {"Super+N", "<Super>n", "notes/shortcutAccelerator", "clipify-notes", "Notes"};
    case Action::Whisper:
        return {"Super+Shift+D", "<Super><Shift>d", "whisper/shortcutAccelerator", "clipify-whisper", "Whisper"};
    case Action::Command:
        return {"", "", "", "", ""};
    case Action::History:
        return {"Super+V", "<Super>v", "clipboard/shortcutAccelerator", "clipify", ""};
    }
    Q_UNREACHABLE();
}

#ifdef CLIPIFY_X11
QString shortcutPath(const ShortcutDefinition &definition)
{
    return QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/")
         + definition.desktopName + '/';
}

constexpr auto customSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:";

QString runSettings(const QStringList &arguments, bool *success = nullptr)
{
    QProcess settings;
    settings.start("gsettings", arguments);
    const bool ok = settings.waitForFinished(1000) && settings.exitStatus() == QProcess::NormalExit
                 && settings.exitCode() == 0;
    if (success)
        *success = ok;
    return ok ? QString::fromUtf8(settings.readAllStandardOutput()).trimmed() : QString();
}

QString customShortcuts()
{
    return runSettings({"get", "org.gnome.settings-daemon.plugins.media-keys", "custom-keybindings"});
}

QString readGnomeShortcut(const QString &path)
{
    const QString binding = runSettings({"get", customSchema + path, "binding"});
    return binding.startsWith('\'') && binding.endsWith('\'') ? binding.sliced(1, binding.size() - 2) : QString();
}

bool gnomeShortcutConfigured(const QString &path)
{
    const auto desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':');
    if (!desktops.contains("GNOME", Qt::CaseInsensitive))
        return false;

    return customShortcuts().contains(path);
}

struct Modifier {
    Qt::KeyboardModifier qt;
    const char *gtk;
    const char *label;
    unsigned int mask;
};
constexpr Modifier modifiers[] = {
    {Qt::ControlModifier, "<Control>", "Ctrl+", ControlMask},
    {Qt::AltModifier, "<Alt>", "Alt+", Mod1Mask},
    {Qt::MetaModifier, "<Super>", "Super+", Mod4Mask},
    {Qt::ShiftModifier, "<Shift>", "Shift+", ShiftMask}
};

QString shortcutLabel(QString accelerator)
{
    QString label;
    for (const auto &modifier : modifiers) {
        if (accelerator.contains(modifier.gtk, Qt::CaseInsensitive)) {
            label += modifier.label;
            accelerator.remove(modifier.gtk, Qt::CaseInsensitive);
        }
    }
    return label + (accelerator == "Print" ? accelerator : accelerator.toUpper());
}

bool grabFailed = false;

int handleGrabError(Display *, XErrorEvent *)
{
    grabFailed = true;
    return 0;
}

unsigned int numLockMask(Display *display)
{
    const KeyCode key = XKeysymToKeycode(display, XK_Num_Lock);
    XModifierKeymap *map = XGetModifierMapping(display);
    unsigned int mask = 0;
    if (map && key) {
        for (int modifier = 0; modifier < 8; ++modifier) {
            for (int index = 0; index < map->max_keypermod; ++index) {
                if (map->modifiermap[modifier * map->max_keypermod + index] == key)
                    mask |= 1u << modifier;
            }
        }
    }
    if (map)
        XFreeModifiermap(map);
    return mask;
}
#endif
}

DesktopShortcut::DesktopShortcut(Action action, QObject *parent)
    : QObject(parent), platform_(std::make_unique<Platform>()),
      shortcut_(QString::fromLatin1(shortcutDefinition(action).label)), action_(action)
{
    auto &config = AppConfig::instance();
    const auto definition = shortcutDefinition(action_);
    const auto seedDefault = [&] {
        if (action_ != Action::Command && !config.value(definition.settingsKey).isValid())
            config.setValue(definition.settingsKey, QString::fromLatin1(definition.accelerator));
    };
#ifdef CLIPIFY_X11
    if (QGuiApplication::platformName() == "xcb")
        platform_->display = XOpenDisplay(nullptr);
    auto *display = platform_->display;
    if (!display) {
        seedDefault();
        return;
    }

    platform_->vKey = XKeysymToKeycode(display, XK_v);
    platform_->controlKey = XKeysymToKeycode(display, XK_Control_L);
    platform_->fakeKeyEvent = reinterpret_cast<Platform::FakeKeyEvent>(
        platform_->xtest.resolve("XTestFakeKeyEvent"));
    using QueryExtension = int (*)(Display *, int *, int *, int *, int *);
    const auto queryExtension = reinterpret_cast<QueryExtension>(
        platform_->xtest.resolve("XTestQueryExtension"));
    int event = 0, error = 0, major = 0, minor = 0;
    if (action_ == Action::History && (!platform_->fakeKeyEvent || !queryExtension
        || !queryExtension(display, &event, &error, &major, &minor)
        || !platform_->vKey || !platform_->controlKey)) {
        seedDefault();
        setError(tr("Super+V requires X11 keyboard and XTEST support."));
        return;
    }

    const QString desktopShortcutPath = shortcutPath(definition);
    platform_->usesGnome = action_ != Action::Command && gnomeShortcutConfigured(desktopShortcutPath);
    if (platform_->usesGnome) {
        auto bus = QDBusConnection::sessionBus();
        const QString suffix = QString::fromLatin1(definition.dbusSuffix);
        const QString object = QStringLiteral("/io/github/riccify/Clipify")
                             + (suffix.isEmpty() ? QString() : '/' + suffix);
        const QString service = QStringLiteral("io.github.riccify.Clipify")
                              + (suffix.isEmpty() ? QString() : '.' + suffix);
        if (bus.registerObject(object, this,
                               QDBusConnection::ExportScriptableInvokables)
            && bus.registerService(service)) {
            platform_->ready = true;
            if (action_ == Action::Whisper) {
                platform_->releasePoll.setInterval(15);
                connect(&platform_->releasePoll, &QTimer::timeout, this, [this] {
                    const QString name = QString(activeAccelerator_).remove(QRegularExpression("<[^>]+>"));
                    const KeyCode key = XKeysymToKeycode(platform_->display, XStringToKeysym(name.toLatin1().constData()));
                    char keys[32]{};
                    XQueryKeymap(platform_->display, keys);
                    if (!key || !(keys[key / 8] & (1 << (key % 8)))) {
                        platform_->releasePoll.stop();
                        emit released();
                    }
                });
            }
            activeAccelerator_ = readGnomeShortcut(desktopShortcutPath);
            shortcut_ = shortcutLabel(activeAccelerator_);
            if (!config.value(definition.settingsKey).isValid())
                config.setValue(definition.settingsKey, activeAccelerator_);
            reloadShortcut();
            connect(&config, &AppConfig::restoringDefaults, this, &DesktopShortcut::releaseForReset);
            connect(&config, &AppConfig::reloaded, this, &DesktopShortcut::reloadShortcut);
            return;
        }

        setError(tr("Clipify could not register its desktop shortcut service. Is it already running?"));
        seedDefault();
        return;
    }

    platform_->ready = true;
    const QString defaultAccelerator = QString::fromLatin1(definition.accelerator);
    QString accelerator = config.value(definition.settingsKey, defaultAccelerator).toString();
    if (action_ == Action::Screenshot && accelerator == "<Control><Shift><Super>s")
        accelerator = defaultAccelerator;
    shortcut_ = shortcutLabel(accelerator);
    if (accelerator.isEmpty() || applyShortcut(accelerator))
        activeAccelerator_ = accelerator;
    else
        setError(tr("The key combination could not be registered. Choose another combination."));
    if (action_ != Action::Command) {
        config.setValue(definition.settingsKey, accelerator);
        connect(&config, &AppConfig::restoringDefaults, this, &DesktopShortcut::releaseForReset);
        connect(&config, &AppConfig::reloaded, this, &DesktopShortcut::reloadShortcut);
    }

    platform_->notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(display), QSocketNotifier::Read);
    connect(platform_->notifier.get(), &QSocketNotifier::activated, this, [this, display] {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (action_ == Action::Whisper && event.type == KeyPress
                && event.xkey.keycode == platform_->shortcutKey) {
                if (!platform_->keyHeld) {
                    platform_->keyHeld = true;
                    emit pressed();
                }
                continue;
            }
            // Open on release so the passive grab no longer owns the keyboard.
            if (event.type != KeyRelease || event.xkey.keycode != platform_->shortcutKey)
                continue;
            if (XPending(display)) {
                XEvent next;
                XPeekEvent(display, &next);
                if (next.type == KeyPress && next.xkey.keycode == event.xkey.keycode
                    && next.xkey.time == event.xkey.time)
                    continue;
            }
            platform_->shortcutTime = event.xkey.time;
            if (action_ == Action::Whisper) {
                platform_->keyHeld = false;
                emit released();
            } else {
                emit activated();
            }
        }
    });
#else
    seedDefault();
#endif
}

DesktopShortcut::~DesktopShortcut() = default;

QVariantMap DesktopShortcut::defaultAccelerators()
{
    QVariantMap defaults;
    for (const auto action : {Action::History, Action::Screenshot, Action::FullScreen,
                              Action::FocusedWindow, Action::Ocr, Action::Notes, Action::Whisper}) {
        const auto definition = shortcutDefinition(action);
        defaults.insert(definition.settingsKey, QString::fromLatin1(definition.accelerator));
    }
    return defaults;
}

bool DesktopShortcut::canChangeShortcut() const
{
#ifdef CLIPIFY_X11
    return platform_->ready;
#else
    return false;
#endif
}

QString DesktopShortcut::acceleratorForKey(int key, int modifierFlags)
{
#ifdef CLIPIFY_X11
    QString accelerator;
    for (const auto &modifier : modifiers) {
        if (modifierFlags & modifier.qt)
            accelerator += modifier.gtk;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        accelerator += "F" + QString::number(key - Qt::Key_F1 + 1);
    } else if (key >= Qt::Key_Space && key <= 0xffff) {
        const auto character = QChar(key).toLower().unicode();
        const KeySym symbol = character <= 0xff ? character : 0x01000000u | character;
        if (const char *name = XKeysymToString(symbol))
            accelerator += QString::fromLatin1(name);
        else
            return {};
    } else {
        switch (key) {
        case Qt::Key_Escape: accelerator += "Escape"; break;
        case Qt::Key_Print:
        case Qt::Key_SysReq: accelerator += "Print"; break;
        case Qt::Key_Return: accelerator += "Return"; break;
        case Qt::Key_Enter: accelerator += "KP_Enter"; break;
        case Qt::Key_Tab: accelerator += "Tab"; break;
        case Qt::Key_Backtab: accelerator += "Tab"; break;
        case Qt::Key_Backspace: accelerator += "BackSpace"; break;
        case Qt::Key_Delete: accelerator += "Delete"; break;
        case Qt::Key_Insert: accelerator += "Insert"; break;
        case Qt::Key_Home: accelerator += "Home"; break;
        case Qt::Key_End: accelerator += "End"; break;
        case Qt::Key_PageUp: accelerator += "Page_Up"; break;
        case Qt::Key_PageDown: accelerator += "Page_Down"; break;
        case Qt::Key_Pause: accelerator += "Pause"; break;
        case Qt::Key_Left: accelerator += "Left"; break;
        case Qt::Key_Right: accelerator += "Right"; break;
        case Qt::Key_Up: accelerator += "Up"; break;
        case Qt::Key_Down: accelerator += "Down"; break;
        default: return {};
        }
    }

    return accelerator;
#else
    Q_UNUSED(key);
    Q_UNUSED(modifierFlags);
    return {};
#endif
}

QString DesktopShortcut::displayAccelerator(const QString &accelerator)
{
#ifdef CLIPIFY_X11
    return shortcutLabel(accelerator);
#else
    return accelerator;
#endif
}

bool DesktopShortcut::bindCommand(const QString &accelerator)
{
    return action_ == Action::Command && canChangeShortcut()
        && accelerator != "<Control>v" && applyShortcut(accelerator);
}

QString DesktopShortcut::recordShortcut(int key, int modifierFlags)
{
#ifdef CLIPIFY_X11
    if (!canChangeShortcut() || action_ == Action::Command)
        return tr("Shortcut recording is unavailable on this desktop.");

    const QString accelerator = acceleratorForKey(key, modifierFlags);
    if (accelerator.isEmpty())
        return tr("This key is unavailable as a global shortcut.");
    if (accelerator == "<Control>v")
        return tr("Ctrl+V is reserved for pasting.");
    if (!applyShortcut(accelerator))
        return tr("That shortcut could not be registered. Choose another combination.");

    if (!AppConfig::instance().setValue(shortcutDefinition(action_).settingsKey, accelerator)) {
        applyShortcut(activeAccelerator_);
        return tr("Could not save the shortcut in config.json.");
    }
    activeAccelerator_ = accelerator;
    shortcut_ = shortcutLabel(accelerator);
    emit shortcutChanged();
    setError({});
    return {};
#else
    Q_UNUSED(key);
    Q_UNUSED(modifierFlags);
    return tr("Shortcut recording is unavailable on this desktop.");
#endif
}

void DesktopShortcut::releaseForReset()
{
#ifdef CLIPIFY_X11
    if (!activeAccelerator_.isEmpty() && applyShortcut({}))
        activeAccelerator_.clear();
#endif
}

void DesktopShortcut::reloadShortcut(bool factoryReset)
{
#ifdef CLIPIFY_X11
    auto &config = AppConfig::instance();
    const auto definition = shortcutDefinition(action_);
    QString requested = config.value(definition.settingsKey).toString();
    if (factoryReset && platform_->usesGnome && !requested.isEmpty()) {
        static const QRegularExpression quotedShortcut("'([^']+)'");
        QStringList systemBindings;
        for (const QString &schema : {QStringLiteral("org.gnome.shell.keybindings"),
                                      QStringLiteral("org.gnome.desktop.wm.keybindings"),
                                      QStringLiteral("org.gnome.mutter.keybindings"),
                                      QStringLiteral("org.gnome.settings-daemon.plugins.media-keys")}) {
            auto bindings = quotedShortcut.globalMatch(runSettings({"list-recursively", schema}));
            while (bindings.hasNext())
                systemBindings.append(shortcutLabel(bindings.next().captured(1)));
        }
        if (systemBindings.contains(shortcutLabel(requested))) {
            const QString fallback = action_ == Action::History ? QStringLiteral("<Super><Shift>v")
                                   : action_ == Action::Notes ? QStringLiteral("<Super><Shift>n") : QString();
            if (fallback.isEmpty() || systemBindings.contains(shortcutLabel(fallback))) {
                config.reportShortcutConflict(shortcutLabel(requested));
                requested.clear();
            } else {
                requested = fallback;
            }
            config.setValue(definition.settingsKey, requested);
        }
    }
    if (requested == activeAccelerator_ && shortcut_ == shortcutLabel(requested))
        return;

    if (!applyShortcut(requested)) {
        config.reportShortcutConflict(shortcutLabel(requested));
        config.setValue(definition.settingsKey, activeAccelerator_);
        shortcut_ = shortcutLabel(activeAccelerator_);
        emit shortcutChanged();
        setError(tr("The requested shortcut could not be registered."));
        return;
    }

    activeAccelerator_ = requested;
    shortcut_ = shortcutLabel(requested);
    emit shortcutChanged();
    setError({});
#else
    Q_UNUSED(factoryReset);
#endif
}

bool DesktopShortcut::applyShortcut(const QString &accelerator)
{
#ifdef CLIPIFY_X11
    auto *display = platform_->display;
    QString keyName = accelerator;
    unsigned int mask = 0;
    for (const auto &modifier : modifiers) {
        if (keyName.contains(modifier.gtk)) {
            keyName.remove(modifier.gtk);
            mask |= modifier.mask;
        }
    }
    const KeyCode key = XKeysymToKeycode(display, XStringToKeysym(keyName.toLatin1().constData()));
    if (!key && !accelerator.isEmpty())
        return false;

    if (platform_->usesGnome) {
        const QString desktopShortcutPath = shortcutPath(shortcutDefinition(action_));
        const QString bindings = customShortcuts();
        static const QRegularExpression pathPattern("'(/[^']+/)'");
        auto paths = pathPattern.globalMatch(bindings);
        while (paths.hasNext()) {
            const QString path = paths.next().captured(1);
            if (!accelerator.isEmpty() && path != desktopShortcutPath
                && shortcutLabel(readGnomeShortcut(path)) == shortcutLabel(accelerator))
                return false;
        }
        bool saved = false;
        runSettings({"set", customSchema + QString(desktopShortcutPath), "binding",
                     "'" + accelerator + "'"}, &saved);
        return saved;
    }

    if (key == platform_->shortcutKey && mask == platform_->shortcutMask)
        return true;

    const unsigned int numLock = numLockMask(display);
    const unsigned int lockMasks[] = {0u, LockMask, numLock, numLock | LockMask};
    const Window root = DefaultRootWindow(display);
    if (accelerator.isEmpty()) {
        if (platform_->shortcutKey) {
            for (const auto locks : lockMasks)
                XUngrabKey(display, platform_->shortcutKey, platform_->shortcutMask | locks, root);
            XFlush(display);
        }
        platform_->shortcutKey = 0;
        platform_->shortcutMask = 0;
        return true;
    }
    grabFailed = false;
    const auto previousHandler = XSetErrorHandler(handleGrabError);
    for (const auto locks : lockMasks)
        XGrabKey(display, key, mask | locks, root, False, GrabModeAsync, GrabModeAsync);
    XSync(display, False);
    XSetErrorHandler(previousHandler);
    if (grabFailed) {
        for (const auto locks : lockMasks)
            XUngrabKey(display, key, mask | locks, root);
        XFlush(display);
        return false;
    }

    if (platform_->shortcutKey) {
        for (const auto locks : lockMasks)
            XUngrabKey(display, platform_->shortcutKey, platform_->shortcutMask | locks, root);
        XFlush(display);
    }
    platform_->shortcutKey = key;
    platform_->shortcutMask = mask;
    return true;
#else
    Q_UNUSED(accelerator);
    return false;
#endif
}

void DesktopShortcut::CaptureScreen()
{
    if (action_ != Action::History && action_ != Action::Notes && action_ != Action::Whisper && action_ != Action::Command)
        emit activated();
}

void DesktopShortcut::ShowHistory()
{
    if (action_ == Action::History)
        activateFromDesktop();
}

void DesktopShortcut::OpenNotes()
{
    if (action_ == Action::Notes)
        activateFromDesktop();
}

void DesktopShortcut::ToggleDictation()
{
#ifdef CLIPIFY_X11
    if (action_ != Action::Whisper || !platform_->usesGnome || platform_->releasePoll.isActive())
        return;
    emit pressed();
    platform_->releasePoll.start();
#endif
}

bool DesktopShortcut::rememberExternalFocus()
{
#ifdef CLIPIFY_X11
    platform_->previousFocus = None;
    if (!platform_->display || !platform_->fakeKeyEvent || !platform_->vKey
        || !platform_->controlKey || QGuiApplication::focusWindow())
        return false;

    int revert = 0;
    XGetInputFocus(platform_->display, &platform_->previousFocus, &revert);
    return platform_->previousFocus != None && platform_->previousFocus != PointerRoot;
#else
    return false;
#endif
}

void DesktopShortcut::activateFromDesktop()
{
#ifdef CLIPIFY_X11
    auto *display = platform_->display;
    if (!display)
        return;

    // Ask X11 for a fresh server timestamp for this desktop-triggered activation.
    const Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                               0, 0, 1, 1, 0, 0, 0);
    XSelectInput(display, window, PropertyChangeMask);
    const unsigned char value = 0;
    XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8, PropModeReplace, &value, 1);
    XEvent event;
    XWindowEvent(display, window, PropertyChangeMask, &event);
    platform_->shortcutTime = event.xproperty.time;
    XDestroyWindow(display, window);
    emit activated();
#endif
}

void DesktopShortcut::showAtPointer(QQuickWindow *window)
{
#ifdef CLIPIFY_X11
    if (!platform_->display)
        return;
    if (!window->isVisible()) {
        int revert = 0;
        XGetInputFocus(platform_->display, &platform_->previousFocus, &revert);
    }
#endif

    const QPoint pointer = QCursor::pos();
    auto *screen = QGuiApplication::screenAt(pointer);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    window->setScreen(screen);
    window->setPosition(std::clamp(pointer.x(), area.left(), std::max(area.left(), area.right() - window->width() + 1)),
                        std::clamp(pointer.y(), area.top(), std::max(area.top(), area.bottom() - window->height() + 1)));
    window->show();
#ifdef CLIPIFY_X11
    // The shortcut is received on a separate X11 connection. Give the
    // window manager that key event's timestamp for focus-stealing checks.
    QGuiApplication::sync();
    auto *display = platform_->display;
    const Window popup = window->winId();
    const Atom activeWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent activation{};
    activation.xclient.type = ClientMessage;
    activation.xclient.window = popup;
    activation.xclient.message_type = activeWindow;
    activation.xclient.format = 32;
    activation.xclient.data.l[0] = 1;
    activation.xclient.data.l[1] = platform_->shortcutTime;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &activation);
    XFlush(display);
#else
    window->requestActivate();
#endif
}

void DesktopShortcut::pasteClipboard()
{
#ifdef CLIPIFY_X11
    auto *display = platform_->display;
    const Window target = platform_->previousFocus;
    if (!display || !platform_->fakeKeyEvent || target == None || target == PointerRoot) {
        setError(tr("The previous window is unavailable. Paste manually with Ctrl+V."));
        return;
    }

    if (action_ != Action::Whisper)
        setError({});
    const QString expected = action_ == Action::Whisper ? QGuiApplication::clipboard()->text() : QString();
    auto *timer = new QTimer(this);
    timer->setInterval(20);
    connect(timer, &QTimer::timeout, this, [this, timer, display, target, expected, attempts = 0]() mutable {
        if (!expected.isEmpty() && QGuiApplication::clipboard()->text() != expected) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        Window focus = None;
        int revert = 0;
        XGetInputFocus(display, &focus, &revert);

        Window root, child;
        int rootX, rootY, windowX, windowY;
        unsigned int modifiers = 0;
        XQueryPointer(display, DefaultRootWindow(display), &root, &child,
                      &rootX, &rootY, &windowX, &windowY, &modifiers);
        if (focus == target && !(modifiers & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask))) {
            platform_->fakeKeyEvent(display, platform_->controlKey, True, CurrentTime);
            platform_->fakeKeyEvent(display, platform_->vKey, True, CurrentTime);
            platform_->fakeKeyEvent(display, platform_->vKey, False, CurrentTime);
            platform_->fakeKeyEvent(display, platform_->controlKey, False, CurrentTime);
            XFlush(display);
            timer->stop();
            timer->deleteLater();
        } else if (++attempts >= 25) {
            if (action_ != Action::Whisper)
                setError(tr("The previous window did not regain focus. Paste manually with Ctrl+V."));
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
#endif
}

void DesktopShortcut::setError(const QString &error)
{
    if (error_ == error)
        return;
    error_ = error;
    emit errorChanged();
}
