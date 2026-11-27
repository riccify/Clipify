#include "ScreenCapture.h"
#include "AppConfig.h"
#include "ClipboardModule.h"

#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <algorithm>
#include <functional>

#ifdef CLIPIFY_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace {
QScreen *screenAtPointer()
{
    auto *screen = QApplication::screenAt(QCursor::pos());
    return screen ? screen : QApplication::primaryScreen();
}

#ifdef CLIPIFY_X11
WId focusedWindow()
{
    const auto *native = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!native || !native->display())
        return 0;

    Display *display = native->display();
    const Atom activeWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    if (activeWindow == None)
        return 0;

    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char *value = nullptr;
    const int result = XGetWindowProperty(display, DefaultRootWindow(display), activeWindow,
                                          0, 1, False, XA_WINDOW, &type, &format,
                                          &count, &remaining, &value);
    const WId window = result == Success && type == XA_WINDOW && format == 32 && count == 1 && value
                     ? *reinterpret_cast<const unsigned long *>(value) : 0;
    if (value)
        XFree(value);
    return window;
}
#endif

QString defaultScreenshotFolder()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.isEmpty() ? QString() : QDir(pictures).filePath("Screenshots");
}

class SelectionOverlay final : public QWidget
{
public:
    SelectionOverlay(QPixmap frame, const QRect &geometry, std::function<void(const QImage &)> selected)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::X11BypassWindowManagerHint),
          frame_(std::move(frame)), selected_(std::move(selected))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setCursor(Qt::CrossCursor);
        setFocusPolicy(Qt::StrongFocus);
        setWindowTitle(tr("Select screenshot area"));
        setGeometry(geometry);
        fade_.setTimerType(Qt::PreciseTimer);
        fade_.setInterval(8);
        connect(&fade_, &QTimer::timeout, this, &SelectionOverlay::updateShade);
    }

    void openSelection()
    {
        show();
        raise();
        grabKeyboard();
        animateShade(1, 175);
    }

    void cancel()
    {
        result_ = {};
        dismiss();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawPixmap(rect(), frame_);
        painter.save();
        painter.setClipRegion(QRegion(rect()).subtracted(selection_));
        painter.fillRect(rect(), QColor(0, 0, 0, qRound(110 * shade_)));
        painter.restore();
        if (!selection_.isEmpty()) {
            QColor border("#ededed");
            if (dismissing_)
                border.setAlphaF(shade_);
            painter.setPen(border);
            painter.drawRect(selection_.adjusted(0, 0, -1, -1));
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
            cancel();
        else
            QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (dismissing_)
            return;

        if (event->button() == Qt::RightButton) {
            cancel();
        } else if (event->button() == Qt::LeftButton) {
            origin_ = event->position().toPoint();
            selecting_ = true;
            updateSelection(origin_);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (selecting_)
            updateSelection(event->position().toPoint());
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (dismissing_ || event->button() != Qt::LeftButton || !selecting_)
            return;
        updateSelection(event->position().toPoint());
        if (selection_.width() < 2 || selection_.height() < 2) {
            cancel();
            return;
        }

        const qreal scaleX = qreal(frame_.width()) / width();
        const qreal scaleY = qreal(frame_.height()) / height();
        const QRect pixels = QRectF(selection_.x() * scaleX, selection_.y() * scaleY,
                                    selection_.width() * scaleX, selection_.height() * scaleY).toAlignedRect();
        result_ = frame_.copy(pixels.intersected(frame_.rect())).toImage();
        dismiss();
    }

private:
    void animateShade(qreal target, int duration)
    {
        initialShade_ = shade_;
        targetShade_ = target;
        fadeDuration_ = duration;
        fadeElapsed_.start();
        fade_.start();
    }

    void updateShade()
    {
        const qreal progress = std::min(qreal(1), qreal(fadeElapsed_.elapsed()) / fadeDuration_);
        const qreal eased = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress);
        shade_ = initialShade_ + (targetShade_ - initialShade_) * eased;
        update();
        if (progress < 1)
            return;

        fade_.stop();
        if (dismissing_) {
            close();
            if (!result_.isNull())
                selected_(result_);
        }
    }

    void dismiss()
    {
        if (dismissing_)
            return;

        dismissing_ = true;
        selecting_ = false;
        animateShade(0, 145);
    }

    void updateSelection(const QPoint &end)
    {
        const QPoint topLeft(std::min(origin_.x(), end.x()), std::min(origin_.y(), end.y()));
        const QPoint bottomRight(std::max(origin_.x(), end.x()), std::max(origin_.y(), end.y()));
        selection_ = QRect(topLeft, bottomRight).intersected(rect());
        update();
    }

    QPixmap frame_;
    std::function<void(const QImage &)> selected_;
    QPoint origin_;
    QRect selection_;
    QImage result_;
    QTimer fade_;
    QElapsedTimer fadeElapsed_;
    qreal initialShade_ = 0;
    qreal targetShade_ = 0;
    qreal shade_ = 0;
    int fadeDuration_ = 0;
    bool selecting_ = false;
    bool dismissing_ = false;
};
}

ScreenCapture::ScreenCapture(ClipboardModule &clipboard) : clipboard_(clipboard)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, [this] {
        emit saveFolderChanged();
        refreshGallery();
    });
}

ScreenCapture::~ScreenCapture()
{
    delete overlay_.data();
}

QString ScreenCapture::saveFolder() const
{
    return AppConfig::instance().value("screen/saveFolder", defaultScreenshotFolder()).toString();
}

void ScreenCapture::chooseSaveFolder()
{
    const QString currentFolder = saveFolder();
    QString startFolder = currentFolder;
    if (!QDir(startFolder).exists() || startFolder.isEmpty())
        startFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!QDir(startFolder).exists() || startFolder.isEmpty())
        startFolder = QDir::homePath();

    const QString folder = QFileDialog::getExistingDirectory(nullptr, tr("Choose screenshot folder"), startFolder);
    if (folder.isEmpty() || folder == currentFolder)
        return;

    if (!AppConfig::instance().setValue("screen/saveFolder", folder))
        return;
    emit saveFolderChanged();
    refreshGallery();
}

void ScreenCapture::begin()
{
    beginSelection(false);
}

void ScreenCapture::beginTextSelection()
{
    beginSelection(true);
}

void ScreenCapture::beginSelection(bool forText)
{
    if (overlay_) {
        static_cast<SelectionOverlay *>(overlay_.data())->cancel();
        return;
    }

    if (QApplication::platformName() != "xcb") {
        qWarning().noquote() << tr("Screen capture currently requires an X11 session.");
        return;
    }

    auto *screen = screenAtPointer();
    if (!screen)
        return;

    const QPixmap frame = screen->grabWindow(0);
    if (frame.isNull()) {
        qWarning().noquote() << tr("Could not capture the screen.");
        return;
    }

    auto *selection = new SelectionOverlay(frame, screen->geometry(), [this, forText](const QImage &image) {
        if (forText)
            emit textAreaSelected(image);
        else
            save(image);
    });
    overlay_ = selection;
    selection->openSelection();
}

void ScreenCapture::captureFullScreen()
{
    if (overlay_ || QApplication::platformName() != "xcb")
        return;

    if (auto *screen = screenAtPointer())
        save(screen->grabWindow(0).toImage());
}

void ScreenCapture::captureFocusedWindow()
{
#ifdef CLIPIFY_X11
    if (overlay_)
        return;

    const WId window = focusedWindow();
    auto *screen = screenAtPointer();
    if (!window || !screen) {
        qWarning().noquote() << tr("No focused window is available to capture.");
        return;
    }
    save(screen->grabWindow(window).toImage());
#endif
}

void ScreenCapture::save(const QImage &image)
{
    if (image.isNull()) {
        qWarning().noquote() << tr("Could not capture a screenshot.");
        return;
    }

    const QString folder = saveFolder();
    const QDir directory(folder);
    if (folder.isEmpty() || !QDir().mkpath(folder)) {
        qWarning().noquote() << tr("Could not create the screenshot folder: %1").arg(folder);
        return;
    }

    int number = settings_.value("screen/lastScreenshotNumber", 0).toInt();
    QString title;
    QFile file;
    do {
        title = QStringLiteral("Screenshot %1").arg(++number);
        file.setFileName(directory.filePath(title + ".png"));
        if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            break;
        if (!file.exists()) {
            qWarning().noquote() << tr("Could not create the screenshot file.");
            return;
        }
    } while (true);

    if (!image.save(&file, "PNG") || !file.flush()) {
        file.remove();
        qWarning().noquote() << tr("Could not save the screenshot.");
        return;
    }
    file.close();
    settings_.setValue("screen/lastScreenshotNumber", number);
    clipboard_.copyScreenshot(image, title);
    refreshGallery();
    emit screenshotSaved(QFileInfo(file.fileName()).fileName());
}

void ScreenCapture::refreshGallery()
{
    QVariantList screenshots;
    const QString folder = saveFolder();
    if (!folder.isEmpty()) {
        const auto files = QDir(folder).entryInfoList({"Screenshot *.png"},
                                                     QDir::Files | QDir::Readable | QDir::NoSymLinks,
                                                     QDir::Time);
        for (const auto &file : files) {
            screenshots.append(QVariantMap{{"name", file.completeBaseName()},
                                           {"url", QUrl::fromLocalFile(file.absoluteFilePath()).toString()}});
        }
    }
    if (screenshots_ != screenshots) {
        screenshots_ = std::move(screenshots);
        emit screenshotsChanged();
    }
}

void ScreenCapture::trashScreenshot(const QString &url)
{
    const auto screenshot = std::find_if(screenshots_.cbegin(), screenshots_.cend(),
                                         [&url](const QVariant &entry) {
                                             return entry.toMap().value("url").toString() == url;
                                         });
    if (screenshot == screenshots_.cend())
        return;

    QFile file(QUrl(url).toLocalFile());
    const QString name = QFileInfo(file.fileName()).fileName();
    if (QFileInfo(file.fileName()).isSymLink() || !file.moveToTrash()) {
        qWarning().noquote() << tr("Could not move %1 to the trash.").arg(name);
        refreshGallery();
        return;
    }

    refreshGallery();
}
