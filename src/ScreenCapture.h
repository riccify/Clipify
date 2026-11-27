#pragma once

#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QVariantList>

class ClipboardModule;
class QImage;
class QWidget;

class ScreenCapture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList screenshots READ screenshots NOTIFY screenshotsChanged)
    Q_PROPERTY(QString saveFolder READ saveFolder NOTIFY saveFolderChanged)

public:
    explicit ScreenCapture(ClipboardModule &clipboard);
    ~ScreenCapture() override;
    QVariantList screenshots() const { return screenshots_; }
    QString saveFolder() const;
    Q_INVOKABLE void chooseSaveFolder();
    Q_INVOKABLE void refreshGallery();
    Q_INVOKABLE void trashScreenshot(const QString &url);
    void begin();
    void beginTextSelection();
    void captureFullScreen();
    void captureFocusedWindow();

signals:
    void screenshotsChanged();
    void saveFolderChanged();
    void screenshotSaved(const QString &fileName);
    void textAreaSelected(const QImage &image);

private:
    void save(const QImage &image);
    void beginSelection(bool forText);

    ClipboardModule &clipboard_;
    QPointer<QWidget> overlay_;
    QSettings settings_;
    QVariantList screenshots_;
};
