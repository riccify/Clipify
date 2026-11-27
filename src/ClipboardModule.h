#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QImage>
#include <QSqlDatabase>
#include <QVector>
#include "HistoryPause.h"

class ClipboardModule : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int historyNumber READ historyNumber WRITE setHistoryNumber NOTIFY historyNumberChanged)
    Q_PROPERTY(int historyLimit READ historyLimit WRITE setHistoryLimit NOTIFY privacySettingsChanged)
    Q_PROPERTY(bool skipSensitive READ skipSensitive WRITE setSkipSensitive NOTIFY privacySettingsChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString storageError READ storageError NOTIFY storageErrorChanged)
    Q_PROPERTY(bool hasOlderEntries READ hasOlderEntries NOTIFY hasOlderEntriesChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentTextChanged)

public:
    explicit ClipboardModule(QObject *parent = nullptr);

    int historyNumber() const;
    void setHistoryNumber(int number);
    int historyLimit() const;
    void setHistoryLimit(int limit);
    bool skipSensitive() const;
    void setSkipSensitive(bool enabled);
    bool paused() const { return historyPause_.paused(); }
    Q_INVOKABLE void pauseHistory(int seconds);
    Q_INVOKABLE void resumeHistory();
    Q_INVOKABLE void clearHistory();
    QString storageError() const;
    bool hasOlderEntries() const;
    QString currentText() const { return currentText_; }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void loadOlderEntries();
    Q_INVOKABLE void setHistorySearch(const QString &text);
    Q_INVOKABLE void copyEntry(int row);
    Q_INVOKABLE bool copyEntryById(const QString &entryId);
    Q_INVOKABLE void previewEntry(const QString &entryId);
    void copyScreenshot(const QImage &image, const QString &title);
    Q_INVOKABLE void deleteEntry(int row);
    Q_INVOKABLE QVariantList recentEntries();
    Q_INVOKABLE bool deleteEntryById(const QString &entryId);

signals:
    void historyNumberChanged();
    void privacySettingsChanged();
    void pausedChanged();
    void historyCleared();
    void storageErrorChanged();
    void hasOlderEntriesChanged();
    void currentTextChanged();

private:
    void recordCopy();
    void reloadHistory();
    bool pruneHistory(qint64 &cutoff);
    void enforceLimit();
    void setStorageError(const QString &message);

    struct Entry {
        qint64 id;
        QString text;
        QDateTime copiedAt;
        bool isImage;
    };

    enum Role { ClipboardText = Qt::UserRole + 1, CopiedAt, IsImage, EntryId };

    HistoryPause historyPause_{"clipboard/pausedIndefinitely"};
    QSqlDatabase database_;
    QString storageError_;
    QString currentText_;
    QVector<Entry> entries_;
    bool hasOlderEntries_ = true;
    QString historySearch_;
};
