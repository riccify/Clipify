#include "ClipboardModule.h"

#include <QClipboard>
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
constexpr auto historyNumberKey = "clipboard/historyNumber";
constexpr int defaultHistoryNumber = 20;
constexpr int historyPageSize = 50;
}

ClipboardModule::ClipboardModule(QObject *parent)
    : QAbstractListModel(parent)
{
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
            copied_at_ms INTEGER NOT NULL
        )
    )")) {
        qWarning() << schema.lastError().text();
        setStorageError(tr("Could not initialize the local history database."));
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
    const int number = settings_.value(historyNumberKey, defaultHistoryNumber).toInt();
    return number > 0 ? number : defaultHistoryNumber;
}

void ClipboardModule::setHistoryNumber(int number)
{
    if (number < 1 || number == historyNumber())
        return;

    settings_.setValue(historyNumberKey, number);
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
    default: return {};
    }
}

QHash<int, QByteArray> ClipboardModule::roleNames() const
{
    return {{ClipboardText, "clipboardText"}, {CopiedAt, "copiedAt"}};
}

void ClipboardModule::loadOlderEntries()
{
    if (!database_.isOpen() || !hasOlderEntries_)
        return;

    QSqlQuery query(database_);
    const QString filter = entries_.isEmpty() ? QString() : QStringLiteral("WHERE id < ? ");
    const QString sql = "SELECT id, text, copied_at_ms FROM clipboard_entries "
                        + filter + "ORDER BY id DESC LIMIT ?";
    if (query.prepare(sql)) {
        if (!entries_.isEmpty())
            query.addBindValue(entries_.last().id);
        query.addBindValue(historyPageSize + 1);
        if (query.exec()) {
            QVector<Entry> page;
            while (page.size() < historyPageSize && query.next()) {
                page.append({query.value(0).toLongLong(), query.value(1).toString(),
                             QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong())});
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

    const QString text = entries_.at(row).text;
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

QVariantList ClipboardModule::recentEntries()
{
    QSqlQuery query(database_);
    if (database_.isOpen() && query.prepare(
            "SELECT id, text FROM clipboard_entries ORDER BY id DESC LIMIT ?")) {
        query.addBindValue(historyNumber());
        if (query.exec()) {
            QVariantList entries;
            while (query.next()) {
                entries.append(QVariantMap{{"id", query.value(0).toString()},
                                           {"text", query.value(1).toString()}});
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
    const QString text = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (currentText_ != text) {
        currentText_ = text;
        emit currentTextChanged();
    }
    if (text.isEmpty() || (!entries_.isEmpty() && entries_.first().text == text))
        return;

    const qint64 copiedAt = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery insert(database_);
    if (insert.prepare("INSERT INTO clipboard_entries (text, copied_at_ms) VALUES (?, ?)")) {
        insert.addBindValue(text);
        insert.addBindValue(copiedAt);
        if (insert.exec()) {
            const bool previouslyHadOlderEntries = hasOlderEntries();
            beginInsertRows({}, 0, 0);
            entries_.prepend({insert.lastInsertId().toLongLong(), text,
                              QDateTime::fromMSecsSinceEpoch(copiedAt)});
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
