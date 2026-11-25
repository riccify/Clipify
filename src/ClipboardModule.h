#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QSettings>
#include <QSqlDatabase>
#include <QVector>

class ClipboardModule : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int historyNumber READ historyNumber WRITE setHistoryNumber NOTIFY historyNumberChanged)
    Q_PROPERTY(QString storageError READ storageError NOTIFY storageErrorChanged)
    Q_PROPERTY(bool hasOlderEntries READ hasOlderEntries NOTIFY hasOlderEntriesChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentTextChanged)

public:
    explicit ClipboardModule(QObject *parent = nullptr);

    int historyNumber() const;
    void setHistoryNumber(int number);
    QString storageError() const;
    bool hasOlderEntries() const;
    QString currentText() const { return currentText_; }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void loadOlderEntries();
    Q_INVOKABLE void copyEntry(int row);
    Q_INVOKABLE void deleteEntry(int row);
    Q_INVOKABLE QVariantList recentEntries();
    Q_INVOKABLE bool deleteEntryById(const QString &entryId);

signals:
    void historyNumberChanged();
    void storageErrorChanged();
    void hasOlderEntriesChanged();
    void currentTextChanged();

private:
    void recordCopy();
    void setStorageError(const QString &message);

    struct Entry {
        qint64 id;
        QString text;
        QDateTime copiedAt;
    };

    enum Role { ClipboardText = Qt::UserRole + 1, CopiedAt };

    QSettings settings_;
    QSqlDatabase database_;
    QString storageError_;
    QString currentText_;
    QVector<Entry> entries_;
    bool hasOlderEntries_ = true;
};
