#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QUrl>
#include <QVariantList>

class Converter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList formats READ formats CONSTANT)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString targetFormat READ targetFormat NOTIFY changed)
    Q_PROPERTY(int readyCount READ readyCount NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY statusChanged)
    Q_PROPERTY(bool savedStatus READ savedStatus NOTIFY statusChanged)

public:
    explicit Converter(QObject *parent = nullptr);
    QVariantList formats() const { return formats_; }
    QVariantList entries() const;
    bool busy() const { return busy_; }
    QString targetFormat() const { return QString::fromLatin1(target_); }
    int readyCount() const;
    QString status() const { return status_; }
    bool failed() const { return failed_; }
    bool savedStatus() const { return savedStatus_; }

    Q_INVOKABLE void chooseFile();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void openFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void openImageData(const QByteArray &bytes);
    Q_INVOKABLE void setTargetFormat(const QString &format);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void save(int index);
    Q_INVOKABLE void saveAll();
    Q_INVOKABLE void finishBatchSave();
    Q_INVOKABLE void copy(int index);

signals:
    void changed();
    void statusChanged();
    void batchSaved();

private:
    struct Result {
        QString sourceFormat;
        QByteArray bytes;
        QString error;
    };

    struct Entry {
        QString name;
        QString path;
        QByteArray input;
        QImage image;
        Result result;
    };

    static Result convert(const Entry &entry, const QByteArray &target);
    QString resultName(const Entry &entry) const;
    void enqueue(Entry entry);
    void startNext();
    void notify(const QString &status, bool failed = false, bool saved = false);

    QFutureWatcher<Result> worker_;
    QVariantList formats_;
    QList<Entry> entries_;
    qsizetype next_ = 0;
    QByteArray target_ = "png";
    QString status_;
    bool busy_ = false;
    bool failed_ = false;
    bool savedStatus_ = false;
    bool batchSaved_ = false;
};
