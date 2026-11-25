#include "ClipboardModule.h"
#include "DesktopShortcut.h"

#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>

class WindowControls : public QObject
{
    Q_OBJECT

public:
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

static QIcon clipifyIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#d6d6d6"));
    painter.drawRoundedRect(QRectF(3, 3, 58, 58), 15, 15);
    painter.setPen(QPen(QColor("#232323"), 3.5));
    painter.drawRoundedRect(QRectF(18, 25, 19, 23), 3, 3);
    painter.drawRoundedRect(QRectF(27, 16, 19, 23), 3, 3);
    return QIcon(pixmap);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Clipify");
    app.setOrganizationName("Clipify");
    QQuickStyle::setStyle("Basic");
    const QIcon icon = clipifyIcon();
    app.setWindowIcon(icon);

    ClipboardModule clipboard;
    DesktopShortcut desktopShortcut;
    WindowControls windowControls;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("clipboardModule", &clipboard);
    engine.rootContext()->setContextProperty("windowControls", &windowControls);
    engine.rootContext()->setContextProperty("desktopShortcut", &desktopShortcut);
    engine.load(QUrl("qrc:/qt/qml/Clipify/qml/Main.qml"));
    if (engine.rootObjects().size() != 1)
        return 1;
    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!mainWindow)
        return 1;

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
    QObject::connect(openAction, &QAction::triggered, mainWindow, openWindow);
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
