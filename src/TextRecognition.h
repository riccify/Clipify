#pragma once

#include <QObject>
#include <QProcess>

class QImage;

class TextRecognition : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accurate READ accurate WRITE setAccurate NOTIFY accurateChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit TextRecognition(QObject *parent = nullptr);
    bool accurate() const;
    void setAccurate(bool accurate);
    QString status() const { return status_; }
    void recognize(const QImage &image);

signals:
    void accurateChanged();
    void statusChanged();

private:
    void setStatus(const QString &status);
    QProcess process_;
    QString status_;
};
