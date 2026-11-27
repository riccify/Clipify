#include "ClipboardModule.h"
#include "AppConfig.h"

#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDialog>
#include <QLabel>
#include <QScreen>
#include <QSqlRecord>
#include <QVBoxLayout>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QMimeData>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <algorithm>

namespace {
constexpr auto imageTitleMime = "application/x-clipify-image-title";
constexpr auto historyNumberKey = "clipboard/historyNumber";
constexpr int defaultHistoryNumber = 20;
constexpr int historyPageSize = 50;
}

ClipboardModule::ClipboardModule(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, &ClipboardModule::historyNumberChanged);
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataPath.isEmpty() || !QDir().mkpath(dataPath)) {
        setStorageError(tr("Could not create the local history folder."));
        return;
    }

    database_ = QSqlDatabase::addDatabase("QSQLITE");
    database_.setDatabaseName(QDir(dataPath).filePath("clipboard.sqlite3"));
    if (!database_.open()) {
        qWarning() << database_.lastError().text();
        setStorageError(tr("Could not open the local history database."));
        return;
    }

    QSqlQuery schema(database_);
    if (!schema.exec(R"(
        CREATE TABLE IF NOT EXISTS clipboard_entries (
            id INTEGER PRIMARY KEY,
            text TEXT NOT NULL,
            copied_at_ms INTEGER NOT NULL,
            image_png BLOB
        )
    )")) {
        qWarning() << schema.lastError().text();
        setStorageError(tr("Could not initialize the local history database."));
        database_.close();
        return;
    }

    if (!database_.record("clipboard_entries").contains("image_png")
        && !schema.exec("ALTER TABLE clipboard_entries ADD COLUMN image_png BLOB")) {
        qWarning() << schema.lastError().text();
        setStorageError(tr("Could not upgrade the local history database."));
        database_.close();
        return;
    }

    auto *clipboard = QGuiApplication::clipboard();
    currentText_ = clipboard->text(QClipboard::Clipboard);
    connect(clipboard, &QClipboard::dataChanged,
            this, &ClipboardModule::recordCopy);
    loadOlderEntries();
}

int ClipboardModule::historyNumber() const
{
    const int number = AppConfig::instance().value(historyNumberKey, defaultHistoryNumber).toInt();
    return number > 0 ? number : defaultHistoryNumber;
}

void ClipboardModule::setHistoryNumber(int number)
{
    if (number < 1 || number == historyNumber())
        return;

    if (!AppConfig::instance().setValue(historyNumberKey, number))
        return;
    emit historyNumberChanged();
}

QString ClipboardModule::storageError() const
{
    return storageError_;
}

bool ClipboardModule::hasOlderEntries() const
{
    return database_.isOpen() && hasOlderEntries_ && !entries_.isEmpty();
}

int ClipboardModule::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

QVariant ClipboardModule::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size())
        return {};

    const auto &entry = entries_.at(index.row());
    switch (role) {
    case ClipboardText: return entry.text;
    case CopiedAt: return entry.copiedAt;
    case IsImage: return entry.isImage;
    case EntryId: return QString::number(entry.id);
    default: return {};
    }
}

QHash<int, QByteArray> ClipboardModule::roleNames() const
{
    return {{ClipboardText, "clipboardText"}, {CopiedAt, "copiedAt"},
            {IsImage, "isImage"}, {EntryId, "entryId"}};
}

void ClipboardModule::setHistorySearch(const QString &text)
{
    const QString search = text.trimmed();
    if (historySearch_ == search)
        return;

    historySearch_ = search;
    beginResetModel();
    entries_.clear();
    hasOlderEntries_ = true;
    endResetModel();
    emit hasOlderEntriesChanged();
    loadOlderEntries();
}

void ClipboardModule::loadOlderEntries()
{
    if (!database_.isOpen() || !hasOlderEntries_)
        return;

    QSqlQuery query(database_);
    QStringList filters;
    if (!entries_.isEmpty())
        filters.append("id < ?");
    if (!historySearch_.isEmpty())
        filters.append("instr(lower(text), lower(?)) > 0");
    const QString filter = filters.isEmpty() ? QString() : "WHERE " + filters.join(" AND ") + ' ';
    const QString sql = "SELECT id, text, copied_at_ms, image_png IS NOT NULL FROM clipboard_entries "
                        + filter + "ORDER BY id DESC LIMIT ?";
    if (query.prepare(sql)) {
        if (!entries_.isEmpty())
            query.addBindValue(entries_.last().id);
        if (!historySearch_.isEmpty())
            query.addBindValue(historySearch_);
        query.addBindValue(historyPageSize + 1);
        if (query.exec()) {
            QVector<Entry> page;
            while (page.size() < historyPageSize && query.next()) {
                page.append({query.value(0).toLongLong(), query.value(1).toString(),
                             QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong()),
                             query.value(3).toBool()});
            }
            hasOlderEntries_ = page.size() == historyPageSize && query.next();
            if (!page.isEmpty()) {
                beginInsertRows({}, entries_.size(), entries_.size() + page.size() - 1);
                entries_.append(page);
                endInsertRows();
            }
            emit hasOlderEntriesChanged();
            setStorageError({});
            return;
        }
    }

    qWarning() << query.lastError().text();
    setStorageError(tr("Could not read the clipboard history."));
}

void ClipboardModule::copyEntry(int row)
{
    if (row < 0 || row >= entries_.size())
        return;

    copyEntryById(QString::number(entries_.at(row).id));
}

bool ClipboardModule::copyEntryById(const QString &entryId)
{
    QSqlQuery query(database_);
    query.prepare("SELECT text, image_png FROM clipboard_entries WHERE id = ?");
    query.addBindValue(entryId);
    if (!query.exec() || !query.next())
        return false;

    const QString title = query.value(0).toString();
    const QByteArray png = query.value(1).toByteArray();
    const bool isImage = !query.isNull(1);
    query.finish();
    if (!isImage) {
        QGuiApplication::clipboard()->setText(title);
    } else {
        const QImage image = QImage::fromData(png, "PNG");
        if (image.isNull())
            return false;
        copyScreenshot(image, title);
    }
    return true;
}

void ClipboardModule::copyScreenshot(const QImage &image, const QString &title)
{
    auto *mime = new QMimeData;
    mime->setImageData(image);
    mime->setData(imageTitleMime, title.toUtf8());
    QGuiApplication::clipboard()->setMimeData(mime);
}

void ClipboardModule::previewEntry(const QString &entryId)
{
    QSqlQuery query(database_);
    query.prepare("SELECT text, image_png FROM clipboard_entries WHERE id = ?");
    query.addBindValue(entryId);
    if (!query.exec() || !query.next())
        return;

    const QPixmap image = QPixmap::fromImage(QImage::fromData(query.value(1).toByteArray(), "PNG"));
    if (image.isNull()) {
        setStorageError(tr("Could not open the clipboard image."));
        return;
    }

    auto *preview = new QDialog;
    preview->setAttribute(Qt::WA_DeleteOnClose);
    preview->setWindowTitle(query.value(0).toString());
    auto *layout = new QVBoxLayout(preview);
    auto *label = new QLabel(preview);
    label->setAlignment(Qt::AlignCenter);
    const auto *screen = QGuiApplication::screenAt(QCursor::pos());
    const QSize limit = screen ? screen->availableGeometry().size() * 0.8 : QSize(900, 650);
    label->setPixmap(image.width() > limit.width() || image.height() > limit.height()
                         ? image.scaled(limit, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image);
    layout->addWidget(label);
    preview->show();
}

QVariantList ClipboardModule::recentEntries()
{
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare(
            "SELECT id, text, image_png IS NOT NULL FROM clipboard_entries ORDER BY id DESC LIMIT ?")) {
        query.addBindValue(historyNumber());
        if (query.exec()) {
            QVariantList entries;
            while (query.next()) {
                entries.append(QVariantMap{{"id", query.value(0).toString()},
                                           {"text", query.value(1).toString()},
                                           {"isImage", query.value(2).toBool()}});
            }
            return entries;
        }
    }

    setStorageError(tr("Could not read the clipboard history."));
    return {};
}

void ClipboardModule::deleteEntry(int row)
{
    if (row < 0 || row >= entries_.size())
        return;

    deleteEntryById(QString::number(entries_.at(row).id));
}

bool ClipboardModule::deleteEntryById(const QString &entryId)
{
    bool valid = false;
    const qint64 id = entryId.toLongLong(&valid);
    if (!valid || !database_.isOpen())
        return false;

    QSqlQuery query(database_);
    if (!query.prepare("DELETE FROM clipboard_entries WHERE id = ?")) {
        qWarning() << query.lastError().text();
        setStorageError(tr("Could not delete the clipboard entry."));
        return false;
    }
    query.addBindValue(id);
    if (!query.exec()) {
        qWarning() << query.lastError().text();
        setStorageError(tr("Could not delete the clipboard entry."));
        return false;
    }

    const auto entry = std::find_if(entries_.cbegin(), entries_.cend(),
                                    [id](const Entry &item) { return item.id == id; });
    if (entry != entries_.cend()) {
        const int row = entry - entries_.cbegin();
        beginRemoveRows({}, row, row);
        entries_.removeAt(row);
        endRemoveRows();
    }
    setStorageError({});
    if (entries_.isEmpty() && hasOlderEntries_)
        loadOlderEntries();
    return true;
}

void ClipboardModule::recordCopy()
{
    auto *clipboard = QGuiApplication::clipboard();
    const QMimeData *mime = clipboard->mimeData();
    const QImage image = mime && mime->hasImage() ? qvariant_cast<QImage>(mime->imageData()) : QImage();
    const bool isImage = !image.isNull();
    const QString currentText = !isImage && mime && mime->hasText() ? mime->text() : QString();
    if (currentText_ != currentText) {
        currentText_ = currentText;
        emit currentTextChanged();
    }

    QString text = currentText;
    QByteArray png;
    if (isImage) {
        text = clipboard->ownsClipboard() ? QString::fromUtf8(mime->data(imageTitleMime)) : QString();
        if (text.isEmpty())
            text = tr("Image");
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            setStorageError(tr("Could not store the clipboard image."));
            return;
        }
    } else if (text.isEmpty()) {
        return;
    }

    const QVariant imageData = isImage ? QVariant(png) : QVariant(QMetaType::fromType<QByteArray>());
    QSqlQuery previous(database_);
    previous.prepare("SELECT 1 FROM (SELECT text, image_png FROM clipboard_entries ORDER BY id DESC LIMIT 1) "
                     "WHERE text = ? AND image_png IS ?");
    previous.addBindValue(text);
    previous.addBindValue(imageData);
    if (previous.exec() && previous.next())
        return;

    const qint64 copiedAt = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery insert(database_);
    if (insert.prepare("INSERT INTO clipboard_entries (text, copied_at_ms, image_png) VALUES (?, ?, ?)")) {
        insert.addBindValue(text);
        insert.addBindValue(copiedAt);
        insert.addBindValue(imageData);
        if (insert.exec()) {
            if (!historySearch_.isEmpty()) {
                QSqlQuery match(database_);
                match.prepare("SELECT instr(lower(?), lower(?)) > 0");
                match.addBindValue(text);
                match.addBindValue(historySearch_);
                if (!match.exec() || !match.next() || !match.value(0).toBool()) {
                    setStorageError({});
                    return;
                }
            }
            const bool previouslyHadOlderEntries = hasOlderEntries();
            beginInsertRows({}, 0, 0);
            entries_.prepend({insert.lastInsertId().toLongLong(), text,
                              QDateTime::fromMSecsSinceEpoch(copiedAt), isImage});
            endInsertRows();
            if (previouslyHadOlderEntries != hasOlderEntries())
                emit hasOlderEntriesChanged();
            setStorageError({});
            return;
        }
    }

    qWarning() << insert.lastError().text();
    setStorageError(tr("Could not save a clipboard entry."));
}

void ClipboardModule::setStorageError(const QString &message)
{
    if (storageError_ == message)
        return;

    storageError_ = message;
    emit storageErrorChanged();
}
