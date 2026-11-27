#include "Converter.h"
#include "AppConfig.h"

#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeData>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrentRun>
#include <algorithm>

namespace {
constexpr auto targetFormatKey = "converter/targetFormat";
QString formatLabel(const QByteArray &format)
{
    return format == "jpg" ? QStringLiteral("JPEG") : QString::fromLatin1(format).toUpper();
}

bool saveImage(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

}

Converter::Converter(QObject *parent) : QObject(parent)
{
    const auto supported = QImageWriter::supportedImageFormats();
    for (const QByteArray format : {"png", "jpg", "webp", "bmp", "tiff"}) {
        if (supported.contains(format))
            formats_.append(QVariantMap{{"label", formatLabel(format)}, {"value", QString::fromLatin1(format)}});
    }

    reloadTargetFormat();
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, &Converter::reloadTargetFormat);

    connect(&worker_, &QFutureWatcher<Result>::finished, this, [this] {
        entries_[next_].result = worker_.result();
        ++next_;
        busy_ = false;
        if (next_ == entries_.size())
            reloadTargetFormat();
        startNext();
    });
}

QVariantList Converter::entries() const
{
    QVariantList rows;
    for (qsizetype index = 0; index < entries_.size(); ++index) {
        const auto &entry = entries_.at(index);
        const auto &result = entry.result;
        rows.append(QVariantMap{{"name", entry.name}, {"sourceFormat", result.sourceFormat},
                                {"ready", !result.bytes.isEmpty()},
                                {"error", result.error}, {"processing", busy_ && index == next_}});
    }
    return rows;
}

int Converter::readyCount() const
{
    return std::count_if(entries_.cbegin(), entries_.cend(), [](const Entry &entry) {
        return !entry.result.bytes.isEmpty();
    });
}

QString Converter::resultName(const Entry &entry) const
{
    const QFileInfo file(entry.name);
    const QString base = file.completeBaseName();
    return (base.isEmpty() ? entry.name : base) + '.' + QString::fromLatin1(target_);
}

void Converter::chooseFile()
{
    QList<QUrl> urls;
    for (const auto &path : QFileDialog::getOpenFileNames(nullptr, tr("Choose images")))
        urls.append(QUrl::fromLocalFile(path));
    if (!urls.isEmpty())
        openFiles(urls);
}

void Converter::paste()
{
    const auto *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime) {
        notify(tr("The clipboard does not contain an image or an image file."), true);
        return;
    }
    const auto urls = mime->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        openFiles(urls);
        return;
    }

    const auto copiedFiles = mime->data("x-special/gnome-copied-files").split('\n');
    if (copiedFiles.size() > 1) {
        QList<QUrl> urls;
        for (const auto &line : copiedFiles.sliced(1)) {
            if (!line.trimmed().isEmpty())
                urls.append(QUrl::fromEncoded(line.trimmed()));
        }
        if (!urls.isEmpty()) {
            openFiles(urls);
            return;
        }
    }

    for (const QByteArray &type : QImageReader::supportedMimeTypes()) {
        if (mime->hasFormat(QString::fromLatin1(type))) {
            const auto bytes = mime->data(QString::fromLatin1(type));
            if (!bytes.isEmpty()) {
                openImageData(bytes);
                return;
            }
        }
    }

    const auto image = qvariant_cast<QImage>(mime->imageData());
    if (!image.isNull()) {
        enqueue({tr("Clipboard image"), {}, {}, image, {}});
        return;
    }

    notify(tr("The clipboard does not contain an image or an image file."), true);
}

void Converter::openFiles(const QList<QUrl> &urls)
{
    int skipped = 0;
    for (const auto &url : urls) {
        const QFileInfo file(url.toLocalFile());
        if (!url.isLocalFile() || !file.isFile()) {
            ++skipped;
            continue;
        }
        entries_.append({file.fileName(), file.absoluteFilePath(), {}, {}, {}});
    }
    if (skipped > 0)
        notify(tr("Skipped %n item(s) that were not local files.", nullptr, skipped), true);
    startNext();
}

void Converter::openImageData(const QByteArray &bytes)
{
    if (!bytes.isEmpty())
        enqueue({tr("Pasted image"), {}, bytes, {}, {}});
}

void Converter::enqueue(Entry entry)
{
    entries_.append(std::move(entry));
    startNext();
}

void Converter::setTargetFormat(const QString &format)
{
    if (busy_ || target_ == format.toLatin1())
        return;
    for (const auto &option : formats_) {
        if (option.toMap().value("value").toString() != format)
            continue;

        if (AppConfig::instance().setValue(targetFormatKey, format))
            reloadTargetFormat();
        return;
    }
}

void Converter::reloadTargetFormat()
{
    if (busy_ || batchSaved_)
        return;
    const auto format = AppConfig::instance().value(targetFormatKey, "png").toString();
    const auto found = std::any_of(formats_.cbegin(), formats_.cend(), [&format](const QVariant &option) {
        return option.toMap().value("value").toString() == format;
    });
    const QByteArray target = found ? format.toLatin1() : QByteArray("png");
    if (target_ == target)
        return;

    target_ = target;
    for (auto &entry : entries_)
        entry.result = {};
    next_ = 0;
    startNext();
}

void Converter::startNext()
{
    if (!busy_ && next_ < entries_.size()) {
        busy_ = true;
        worker_.setFuture(QtConcurrent::run(&Converter::convert, entries_.at(next_), target_));
    }
    emit changed();
}

void Converter::remove(int index)
{
    if (busy_ || index < 0 || index >= entries_.size())
        return;

    entries_.removeAt(index);
    next_ = entries_.size();
    emit changed();
}

void Converter::clear()
{
    if (busy_ || batchSaved_)
        return;

    entries_.clear();
    next_ = 0;
    emit changed();
    notify({});
}

Converter::Result Converter::convert(const Entry &entry, const QByteArray &target)
{
    Result result;
    QImage image = entry.image;
    if (image.isNull()) {
        QBuffer buffer;
        QImageReader reader;
        if (entry.path.isEmpty()) {
            buffer.setData(entry.input);
            buffer.open(QIODevice::ReadOnly);
            reader.setDevice(&buffer);
        } else {
            reader.setFileName(entry.path);
        }
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(true);
        result.sourceFormat = formatLabel(reader.format());
        image = reader.read();
        if (image.isNull()) {
            result.error = tr("Could not read this image: %1").arg(reader.errorString());
            return result;
        }
    } else {
        result.sourceFormat = tr("Image");
    }

    if (target == "jpg" && image.hasAlphaChannel()) {
        QImage flattened(image.size(), QImage::Format_RGB32);
        if (flattened.isNull()) {
            result.error = tr("Could not prepare the image for JPEG conversion.");
            return result;
        }
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, image);
        painter.end();
        image = flattened;
    }

    QBuffer encoded(&result.bytes);
    encoded.open(QIODevice::WriteOnly);
    QImageWriter writer(&encoded, target);
    if (!writer.write(image)) {
        result.bytes.clear();
        result.error = tr("Could not create the converted image: %1").arg(writer.errorString());
    }
    return result;
}

void Converter::save(int index)
{
    if (busy_ || index < 0 || index >= entries_.size() || entries_.at(index).result.bytes.isEmpty())
        return;
    const auto entry = entries_.at(index);
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QFileDialog dialog(nullptr, tr("Save converted image"), QDir(folder).filePath(resultName(entry)),
                       QStringLiteral("%1 (*.%2)").arg(formatLabel(target_), QString::fromLatin1(target_)));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QString::fromLatin1(target_));
    if (dialog.exec() != QDialog::Accepted)
        return;

    const bool saved = saveImage(dialog.selectedFiles().first(), entry.result.bytes);
    notify(saved ? tr("Image saved.") : tr("Could not save the converted image."), !saved, saved);
}

void Converter::saveAll()
{
    if (busy_ || batchSaved_ || readyCount() == 0)
        return;
    const QString path = QFileDialog::getExistingDirectory(nullptr, tr("Save converted images"),
                                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    if (path.isEmpty())
        return;

    const QDir folder(path);
    int saved = 0;
    for (const auto &entry : entries_) {
        if (entry.result.bytes.isEmpty())
            continue;
        const QFileInfo name(resultName(entry));
        QString output = folder.filePath(name.fileName());
        for (int suffix = 2; QFileInfo::exists(output); ++suffix) {
            output = folder.filePath(QStringLiteral("%1 (%2).%3")
                .arg(name.completeBaseName()).arg(suffix).arg(name.suffix()));
        }
        if (saveImage(output, entry.result.bytes))
            ++saved;
    }
    if (saved == entries_.size()) {
        batchSaved_ = true;
        notify({});
        emit batchSaved();
        return;
    }

    const auto unsaved = entries_.size() - saved;
    notify(tr("Saved %1 image(s). %2 could not be exported.").arg(saved).arg(unsaved), true);
}

void Converter::finishBatchSave()
{
    if (!batchSaved_)
        return;

    const bool single = entries_.size() == 1;
    batchSaved_ = false;
    entries_.clear();
    next_ = 0;
    reloadTargetFormat();
    emit changed();
    notify(single ? tr("Image saved.") : tr("Images saved."), false, true);
}

void Converter::copy(int index)
{
    if (index < 0 || index >= entries_.size())
        return;
    const auto &bytes = entries_.at(index).result.bytes;
    if (bytes.isEmpty())
        return;

    const QString type = target_ == "jpg" ? QStringLiteral("image/jpeg")
                                         : QStringLiteral("image/") + QString::fromLatin1(target_);
    auto *mime = new QMimeData;
    mime->setImageData(QImage::fromData(bytes, target_.constData()));
    mime->setData(type, bytes);
    QGuiApplication::clipboard()->setMimeData(mime);
    notify(tr("Image copied."));
}

void Converter::notify(const QString &status, bool failed, bool saved)
{
    status_ = status;
    failed_ = failed;
    savedStatus_ = saved;
    emit statusChanged();
}
