#include "DesktopShortcut.h"

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
    std::unique_ptr<QSocketNotifier> notifier;

    ~Platform()
    {
        notifier.reset();
        if (display)
            XCloseDisplay(display);
    }
#endif
};

#ifdef CLIPIFY_X11
namespace {
constexpr auto desktopShortcutPath = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/clipify/";
constexpr auto customSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:";
constexpr auto shortcutSetting = "clipboard/shortcutAccelerator";
constexpr auto defaultAccelerator = "<Super>v";

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

bool gnomeShortcutConfigured()
{
    const auto desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':');
    if (!desktops.contains("GNOME", Qt::CaseInsensitive))
        return false;

    return customShortcuts().contains(desktopShortcutPath);
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
    {Qt::ShiftModifier, "<Shift>", "Shift+", ShiftMask},
    {Qt::MetaModifier, "<Super>", "Super+", Mod4Mask}
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
    return label + accelerator.toUpper();
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
}
#endif

DesktopShortcut::DesktopShortcut(QObject *parent)
    : QObject(parent), platform_(std::make_unique<Platform>())
{
#ifdef CLIPIFY_X11
    if (QGuiApplication::platformName() == "xcb")
        platform_->display = XOpenDisplay(nullptr);
    auto *display = platform_->display;
    if (!display)
        return;

    platform_->vKey = XKeysymToKeycode(display, XK_v);
    platform_->controlKey = XKeysymToKeycode(display, XK_Control_L);
    platform_->fakeKeyEvent = reinterpret_cast<Platform::FakeKeyEvent>(
        platform_->xtest.resolve("XTestFakeKeyEvent"));
    using QueryExtension = int (*)(Display *, int *, int *, int *, int *);
    const auto queryExtension = reinterpret_cast<QueryExtension>(
        platform_->xtest.resolve("XTestQueryExtension"));
    int event = 0, error = 0, major = 0, minor = 0;
    if (!platform_->fakeKeyEvent || !queryExtension
        || !queryExtension(display, &event, &error, &major, &minor)
        || !platform_->vKey || !platform_->controlKey) {
        setError(tr("Super+V requires X11 keyboard and XTEST support."));
        return;
    }

    platform_->usesGnome = gnomeShortcutConfigured();
    if (platform_->usesGnome) {
        auto bus = QDBusConnection::sessionBus();
        if (bus.registerObject("/io/github/riccify/Clipify", this,
                               QDBusConnection::ExportScriptableInvokables)
            && bus.registerService("io.github.riccify.Clipify")) {
            platform_->ready = true;
            shortcut_ = shortcutLabel(readGnomeShortcut(desktopShortcutPath));
            return;
        }

        setError(tr("Clipify could not register its desktop shortcut service. Is it already running?"));
        return;
    }

    platform_->ready = true;
    const QString accelerator = settings_.value(shortcutSetting, defaultAccelerator).toString();
    shortcut_ = shortcutLabel(accelerator);
    if (!applyShortcut(accelerator))
        setError(tr("The clipboard shortcut could not be registered. Choose another combination."));

    platform_->notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(display), QSocketNotifier::Read);
    connect(platform_->notifier.get(), &QSocketNotifier::activated, this, [this, display] {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
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
            emit activated();
        }
    });
#endif
}

DesktopShortcut::~DesktopShortcut() = default;

bool DesktopShortcut::canChangeShortcut() const
{
#ifdef CLIPIFY_X11
    return platform_->ready;
#else
    return false;
#endif
}

QString DesktopShortcut::recordShortcut(int key, int modifierFlags)
{
#ifdef CLIPIFY_X11
    if (!canChangeShortcut())
        return tr("Shortcut recording is unavailable on this desktop.");

    QString accelerator;
    for (const auto &modifier : modifiers) {
        if (modifierFlags & modifier.qt)
            accelerator += modifier.gtk;
    }
    if (!(modifierFlags & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
        return tr("Hold Ctrl, Alt or Super while pressing a key.");

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        accelerator += QChar(key).toLower();
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        accelerator += "F" + QString::number(key - Qt::Key_F1 + 1);
    } else if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
        accelerator += QString::fromLatin1(XKeysymToString(key));
    } else {
        switch (key) {
        case Qt::Key_Return: accelerator += "Return"; break;
        case Qt::Key_Tab: accelerator += "Tab"; break;
        case Qt::Key_Left: accelerator += "Left"; break;
        case Qt::Key_Right: accelerator += "Right"; break;
        case Qt::Key_Up: accelerator += "Up"; break;
        case Qt::Key_Down: accelerator += "Down"; break;
        default: return tr("Choose a letter, number, function key or arrow key.");
        }
    }

    if (accelerator == "<Control>v")
        return tr("Ctrl+V is reserved for pasting.");
    if (!applyShortcut(accelerator))
        return tr("That shortcut could not be registered. Choose another combination.");

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

bool DesktopShortcut::applyShortcut(const QString &accelerator)
{
#ifdef CLIPIFY_X11
    if (platform_->usesGnome) {
        const QString bindings = customShortcuts();
        static const QRegularExpression pathPattern("'(/[^']+/)'");
        auto paths = pathPattern.globalMatch(bindings);
        while (paths.hasNext()) {
            const QString path = paths.next().captured(1);
            if (path != desktopShortcutPath && shortcutLabel(readGnomeShortcut(path)) == shortcutLabel(accelerator))
                return false;
        }
        bool saved = false;
        runSettings({"set", customSchema + QString(desktopShortcutPath), "binding",
                     "'" + accelerator + "'"}, &saved);
        return saved;
    }

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
    if (!key || !mask)
        return false;
    if (key == platform_->shortcutKey && mask == platform_->shortcutMask)
        return true;

    const unsigned int numLock = numLockMask(display);
    const unsigned int lockMasks[] = {0u, LockMask, numLock, numLock | LockMask};
    const Window root = DefaultRootWindow(display);
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
    settings_.setValue(shortcutSetting, accelerator);
    return true;
#else
    Q_UNUSED(accelerator);
    return false;
#endif
}

void DesktopShortcut::ShowHistory()
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

void DesktopShortcut::paste(const QString &text)
{
#ifdef CLIPIFY_X11
    QGuiApplication::clipboard()->setText(text);
    auto *display = platform_->display;
    const Window target = platform_->previousFocus;
    if (!display || !platform_->fakeKeyEvent || target == None || target == PointerRoot) {
        setError(tr("The previous window is unavailable. Paste manually with Ctrl+V."));
        return;
    }

    setError({});
    auto *timer = new QTimer(this);
    timer->setInterval(20);
    connect(timer, &QTimer::timeout, this, [this, timer, display, target, attempts = 0]() mutable {
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
            setError(tr("The previous window did not regain focus. Paste manually with Ctrl+V."));
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
#else
    Q_UNUSED(text);
#endif
}

void DesktopShortcut::setError(const QString &error)
{
    if (error_ == error)
        return;
    error_ = error;
    emit errorChanged();
}
