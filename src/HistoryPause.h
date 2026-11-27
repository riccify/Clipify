#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class HistoryPause : public QObject
{
    Q_OBJECT

public:
    explicit HistoryPause(const QString &settingsKey);
    bool paused() const { return paused_; }
    void pause(int seconds);
    void resume();

signals:
    void changed();

private:
    QString settingsKey_;
    QTimer timer_;
    bool paused_;
};
