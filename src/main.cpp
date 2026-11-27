#include "ClipboardModule.h"
#include "DesktopShortcut.h"
#include "ScreenCapture.h"
#include "TextRecognition.h"
#include "ReleaseCheck.h"
#include "QuickNotes.h"
#include "AppConfig.h"
#include "Converter.h"
#include "CommandShortcuts.h"
#include "WhisperModule.h"
#include "LlmService.h"

#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>

#ifdef CLIPIFY_X11
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#endif

class WindowControls : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.riccify.Clipify.Application")

signals:
    void openWindowRequested();

public:
    Q_SCRIPTABLE Q_INVOKABLE void OpenWindow()
    {
        emit openWindowRequested();
    }

    Q_INVOKABLE bool trayAvailable() const
    {
        return QSystemTrayIcon::isSystemTrayAvailable();
    }

    Q_INVOKABLE void quit() const
    {
        if (!quitBlocked_)
            QCoreApplication::exit(0);
    }

    void setQuitBlocked(bool blocked) { quitBlocked_ = blocked; }

    Q_INVOKABLE void move(QQuickWindow *window) const
    {
        window->startSystemMove();
    }

    Q_INVOKABLE void resize(QQuickWindow *window, int edges) const
    {
        window->startSystemResize(Qt::Edges::fromInt(edges));
    }

private:
    bool quitBlocked_ = false;
};

#ifdef CLIPIFY_X11
static QString configureGnomeShortcuts()
{
    if (QGuiApplication::platformName() != "xcb"
        || !qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':').contains("GNOME", Qt::CaseInsensitive))
        return {};

    QProcess setup;
    setup.start("/usr/bin/python3",
                {QCoreApplication::applicationDirPath() + "/clipify-setup-gnome"});
    if (!setup.waitForFinished(3000) || setup.exitStatus() != QProcess::NormalExit || setup.exitCode() != 0) {
        if (setup.state() != QProcess::NotRunning) {
            setup.kill();
            setup.waitForFinished();
        }
        qWarning().noquote() << setup.readAllStandardError();
        return QApplication::translate("Shortcuts", "Could not complete GNOME shortcut setup.");
    }
    return QString::fromUtf8(setup.readAllStandardError()).trimmed();
}
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("Clipify");
    app.setApplicationVersion(CLIPIFY_VERSION);
    app.setDesktopFileName("io.github.riccify.Clipify");
    app.setOrganizationName("Clipify");
    const bool startInTray = app.arguments().contains("--background");
    QString gnomeSetupWarning;
    WindowControls windowControls;
#ifdef CLIPIFY_X11
    const QString applicationService = "io.github.riccify.Clipify.Application";
    const QString applicationObject = "/io/github/riccify/Clipify/Application";
    auto sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        if (!sessionBus.registerService(applicationService)) {
            if (!startInTray) {
                const auto open = QDBusMessage::createMethodCall(applicationService, applicationObject,
                                                                 applicationService, "OpenWindow");
                return sessionBus.call(open, QDBus::Block, 2000).type() == QDBusMessage::ReplyMessage ? 0 : 1;
            }
            return 0;
        }
        if (!sessionBus.registerObject(applicationObject, &windowControls,
                                       QDBusConnection::ExportScriptableInvokables))
            return 1;
        gnomeSetupWarning = configureGnomeShortcuts();
    }
#endif
    auto &appConfig = AppConfig::instance();
    QQuickStyle::setStyle("Basic");
    const QIcon icon(":/clipify/assets/clipify-app-icon.png");
    app.setWindowIcon(icon);

    ClipboardModule clipboard;
    DesktopShortcut desktopShortcut;
    DesktopShortcut pauseHistoryShortcut(DesktopShortcut::Action::PauseHistory);
    DesktopShortcut screenShortcut(DesktopShortcut::Action::Screenshot);
    DesktopShortcut fullScreenShortcut(DesktopShortcut::Action::FullScreen);
    DesktopShortcut focusedWindowShortcut(DesktopShortcut::Action::FocusedWindow);
    DesktopShortcut ocrShortcut(DesktopShortcut::Action::Ocr);
    DesktopShortcut notesShortcut(DesktopShortcut::Action::Notes);
    QuickNotes quickNotes;
    Converter converter;
    CommandShortcuts commandShortcuts;
    DesktopShortcut whisperShortcut(DesktopShortcut::Action::Whisper);
    DesktopShortcut pauseDictationShortcut(DesktopShortcut::Action::PauseDictation);
    LlmService llmService;
    WhisperModule whisperModule(llmService);
    bool pasteDictation = false;
    bool stopOnRelease = false;
    QObject::connect(&whisperShortcut, &DesktopShortcut::pressed, &whisperModule, [&] {
        if (!whisperModule.busy()) {
            pasteDictation = whisperShortcut.rememberExternalFocus();
            stopOnRelease = whisperModule.holdToTalk();
            whisperModule.toggleRecording();
        } else if (whisperModule.recording() && !stopOnRelease && !whisperModule.holdToTalk()) {
            whisperModule.toggleRecording();
        }
    });
    QObject::connect(&whisperShortcut, &DesktopShortcut::released, &whisperModule, [&] {
        if (stopOnRelease && whisperModule.recording())
            whisperModule.toggleRecording();
    });
    QObject::connect(&whisperModule, &WhisperModule::transcriptReady, &whisperShortcut, [&] {
        if (pasteDictation)
            whisperShortcut.pasteClipboard();
    });
    QObject::connect(&whisperModule, &WhisperModule::changed, &whisperShortcut, [&] {
        if (!whisperModule.busy()) {
            pasteDictation = false;
            stopOnRelease = false;
        }
    });
    ScreenCapture screenCapture(clipboard);
    TextRecognition textRecognition;
    QObject::connect(&ocrShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::beginTextSelection);
    QObject::connect(&screenCapture, &ScreenCapture::textAreaSelected, &textRecognition, &TextRecognition::recognize);
    QObject::connect(&screenShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::begin);
    QObject::connect(&fullScreenShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::captureFullScreen);
    QObject::connect(&focusedWindowShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::captureFocusedWindow);
    ReleaseCheck releaseCheck;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("releaseCheck", &releaseCheck);
    engine.rootContext()->setContextProperty("startInTray", startInTray);
    engine.rootContext()->setContextProperty("gnomeSetupWarning", gnomeSetupWarning);
    engine.rootContext()->setContextProperty("clipboardModule", &clipboard);
    engine.rootContext()->setContextProperty("windowControls", &windowControls);
    engine.rootContext()->setContextProperty("desktopShortcut", &desktopShortcut);
    engine.rootContext()->setContextProperty("pauseHistoryShortcut", &pauseHistoryShortcut);
    engine.rootContext()->setContextProperty("screenShortcut", &screenShortcut);
    engine.rootContext()->setContextProperty("fullScreenShortcut", &fullScreenShortcut);
    engine.rootContext()->setContextProperty("focusedWindowShortcut", &focusedWindowShortcut);
    engine.rootContext()->setContextProperty("screenCapture", &screenCapture);
    engine.rootContext()->setContextProperty("ocrShortcut", &ocrShortcut);
    engine.rootContext()->setContextProperty("textRecognition", &textRecognition);
    engine.rootContext()->setContextProperty("notesShortcut", &notesShortcut);
    engine.rootContext()->setContextProperty("quickNotes", &quickNotes);
    engine.rootContext()->setContextProperty("appConfig", &appConfig);
    engine.rootContext()->setContextProperty("converter", &converter);
    engine.rootContext()->setContextProperty("commandShortcuts", &commandShortcuts);
    engine.rootContext()->setContextProperty("whisperShortcut", &whisperShortcut);
    engine.rootContext()->setContextProperty("pauseDictationShortcut", &pauseDictationShortcut);
    engine.rootContext()->setContextProperty("whisperModule", &whisperModule);
    engine.rootContext()->setContextProperty("llmService", &llmService);
    engine.load(QUrl("qrc:/qt/qml/Clipify/qml/Main.qml"));
    if (engine.rootObjects().size() != 1)
        return 1;
    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!mainWindow)
        return 1;
    mainWindow->setIcon(icon);

    engine.load(QUrl("qrc:/qt/qml/Clipify/qml/HistoryPopup.qml"));
    if (engine.rootObjects().size() != 2)
        return 1;

    engine.load(QUrl("qrc:/qt/qml/Clipify/qml/QuickNotePopup.qml"));
    if (engine.rootObjects().size() != 3)
        return 1;

    engine.load(QUrl("qrc:/qt/qml/Clipify/qml/WhisperOverlay.qml"));
    if (engine.rootObjects().size() != 4)
        return 1;

    QMenu trayMenu;
    auto *openAction = trayMenu.addAction(QApplication::translate("Tray", "Open Window"));
    const auto addHistoryMenu = [&trayMenu]<typename History>(
        History &history, const QString &pauseTitle, const QString &pausedTitle) {
        auto *menu = trayMenu.addMenu(history.paused() ? pausedTitle : pauseTitle);
        const auto addPause = [&](const char *label, int seconds) {
            auto *action = menu->addAction(QApplication::translate("Tray", label));
            QObject::connect(action, &QAction::triggered, &history, [&history, seconds] {
                history.pauseHistory(seconds);
            });
        };
        addPause("30 seconds", 30);
        addPause("1 minute", 60);
        addPause("10 minutes", 600);
        addPause("30 minutes", 1800);
        addPause("Until resumed", 0);
        auto *resume = menu->addAction(QApplication::translate("Tray", "Resume history"));
        resume->setVisible(history.paused());
        QObject::connect(resume, &QAction::triggered, &history, &History::resumeHistory);
        QObject::connect(&history, &History::pausedChanged, menu, [&history, menu, resume, pauseTitle, pausedTitle] {
            resume->setVisible(history.paused());
            menu->setTitle(history.paused() ? pausedTitle : pauseTitle);
        });
    };
    addHistoryMenu(clipboard, QApplication::translate("Tray", "Pause history"),
                   QApplication::translate("Tray", "History paused"));
    addHistoryMenu(whisperModule, QApplication::translate("Tray", "Pause dictation history"),
                   QApplication::translate("Tray", "Dictation history paused"));
    trayMenu.addSeparator();
    auto *quitAction = trayMenu.addAction(QApplication::translate("Tray", "Quit"));
    QObject::connect(&releaseCheck, &ReleaseCheck::changed, &windowControls, [&] {
        windowControls.setQuitBlocked(releaseCheck.busy());
        quitAction->setEnabled(!releaseCheck.busy());
    });
    const auto openWindow = [mainWindow] {
        if (mainWindow->visibility() == QWindow::Minimized)
            mainWindow->setWindowState(Qt::WindowNoState);
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();
    };

    QSystemTrayIcon tray(icon);
    tray.setToolTip("Clipify");
    tray.setContextMenu(&trayMenu);
    const auto notifyDesktop = [&tray](const QString &title, const QString &body, const QString &iconName) {
#ifdef CLIPIFY_X11
        auto bus = QDBusConnection::sessionBus();
        if (bus.isConnected()) {
            auto notification = QDBusMessage::createMethodCall("org.freedesktop.Notifications",
                "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
            notification << QStringLiteral("Clipify") << uint(0) << iconName
                         << title << body << QStringList{} << QVariantMap{} << 4000;
            bus.asyncCall(notification);
            return;
        }
#else
        Q_UNUSED(iconName);
#endif
        tray.showMessage(title, body, QSystemTrayIcon::Information, 4000);
    };
    QObject::connect(&screenCapture, &ScreenCapture::screenshotSaved, &tray,
                     [notifyDesktop](const QString &fileName) {
        const QString title = QApplication::translate("Screen", "Screenshot saved");
        const QString body = QApplication::translate("Screen", "%1 copied to the clipboard.").arg(fileName);
        notifyDesktop(title, body, QStringLiteral("camera-photo"));
    });
    QObject::connect(&pauseHistoryShortcut, &DesktopShortcut::activated, &tray, [&clipboard, notifyDesktop] {
        if (clipboard.paused()) {
            clipboard.resumeHistory();
            notifyDesktop(QApplication::translate("Clipboard", "Clipboard history resumed"),
                          QApplication::translate("Clipboard", "New copies will be saved to the history."),
                          QStringLiteral("io.github.riccify.Clipify"));
        } else {
            clipboard.pauseHistory(0);
            notifyDesktop(QApplication::translate("Clipboard", "Clipboard history paused"),
                          QApplication::translate("Clipboard", "New copies will not be saved until you resume the history."),
                          QStringLiteral("io.github.riccify.Clipify"));
        }
    });
    QObject::connect(&pauseDictationShortcut, &DesktopShortcut::activated, &tray, [&whisperModule, notifyDesktop] {
        if (whisperModule.paused()) {
            whisperModule.resumeHistory();
            notifyDesktop(QApplication::translate("Whisper", "Dictation history resumed"),
                          QApplication::translate("Whisper", "New dictations will be saved to the history."),
                          QStringLiteral("io.github.riccify.Clipify"));
        } else {
            whisperModule.pauseHistory(0);
            notifyDesktop(QApplication::translate("Whisper", "Dictation history paused"),
                          QApplication::translate("Whisper", "New dictations will not be saved until you resume the history."),
                          QStringLiteral("io.github.riccify.Clipify"));
        }
    });
    QObject::connect(openAction, &QAction::triggered, mainWindow, openWindow);
    QObject::connect(&windowControls, &WindowControls::openWindowRequested, mainWindow, openWindow);
    QObject::connect(quitAction, &QAction::triggered, &windowControls, &WindowControls::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, mainWindow,
                     [openWindow](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                             openWindow();
                     });
    tray.show();
    releaseCheck.check();

    return app.exec();
}

#include "main.moc"
