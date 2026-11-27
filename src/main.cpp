#include "ClipboardModule.h"
#include "DesktopShortcut.h"
#include "ScreenCapture.h"
#include "TextRecognition.h"

#include <QApplication>
#include <QIcon>
#include <QMenu>
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
        QCoreApplication::exit(0);
    }

    Q_INVOKABLE void move(QQuickWindow *window) const
    {
        window->startSystemMove();
    }

    Q_INVOKABLE void resize(QQuickWindow *window, int edges) const
    {
        window->startSystemResize(Qt::Edges::fromInt(edges));
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("Clipify");
    app.setDesktopFileName("io.github.riccify.Clipify");
    app.setOrganizationName("Clipify");
    const bool startInTray = app.arguments().contains("--background");
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
    }
#endif
    QQuickStyle::setStyle("Basic");
    const QIcon icon(":/clipify/assets/clipify-app-icon.png");
    app.setWindowIcon(icon);

    ClipboardModule clipboard;
    DesktopShortcut desktopShortcut;
    DesktopShortcut screenShortcut(DesktopShortcut::Action::Screenshot);
    DesktopShortcut fullScreenShortcut(DesktopShortcut::Action::FullScreen);
    DesktopShortcut focusedWindowShortcut(DesktopShortcut::Action::FocusedWindow);
    DesktopShortcut ocrShortcut(DesktopShortcut::Action::Ocr);
    ScreenCapture screenCapture(clipboard);
    TextRecognition textRecognition;
    QObject::connect(&ocrShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::beginTextSelection);
    QObject::connect(&screenCapture, &ScreenCapture::textAreaSelected, &textRecognition, &TextRecognition::recognize);
    QObject::connect(&screenShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::begin);
    QObject::connect(&fullScreenShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::captureFullScreen);
    QObject::connect(&focusedWindowShortcut, &DesktopShortcut::activated, &screenCapture, &ScreenCapture::captureFocusedWindow);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("startInTray", startInTray);
    engine.rootContext()->setContextProperty("clipboardModule", &clipboard);
    engine.rootContext()->setContextProperty("windowControls", &windowControls);
    engine.rootContext()->setContextProperty("desktopShortcut", &desktopShortcut);
    engine.rootContext()->setContextProperty("screenShortcut", &screenShortcut);
    engine.rootContext()->setContextProperty("fullScreenShortcut", &fullScreenShortcut);
    engine.rootContext()->setContextProperty("focusedWindowShortcut", &focusedWindowShortcut);
    engine.rootContext()->setContextProperty("screenCapture", &screenCapture);
    engine.rootContext()->setContextProperty("ocrShortcut", &ocrShortcut);
    engine.rootContext()->setContextProperty("textRecognition", &textRecognition);
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

    QMenu trayMenu;
    auto *openAction = trayMenu.addAction(QApplication::translate("Tray", "Open Window"));
    trayMenu.addSeparator();
    auto *quitAction = trayMenu.addAction(QApplication::translate("Tray", "Quit"));
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
    QObject::connect(&screenCapture, &ScreenCapture::screenshotSaved, &tray,
                     [&tray](const QString &fileName) {
        const QString title = QApplication::translate("Screen", "Screenshot saved");
        const QString body = QApplication::translate("Screen", "%1 copied to the clipboard.").arg(fileName);
#ifdef CLIPIFY_X11
        auto bus = QDBusConnection::sessionBus();
        if (bus.isConnected()) {
            auto notification = QDBusMessage::createMethodCall("org.freedesktop.Notifications",
                "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
            notification << QStringLiteral("Clipify") << uint(0) << QStringLiteral("camera-photo")
                         << title << body << QStringList{} << QVariantMap{} << 4000;
            bus.asyncCall(notification);
            return;
        }
#endif
        tray.showMessage(title, body, QSystemTrayIcon::Information, 4000);
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

    return app.exec();
}

#include "main.moc"
