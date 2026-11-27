#include "HistoryPause.h"

#include <QSettings>

HistoryPause::HistoryPause(const QString &settingsKey)
    : settingsKey_(settingsKey), paused_(QSettings().value(settingsKey, false).toBool())
{
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &HistoryPause::resume);
}

void HistoryPause::pause(int seconds)
{
    if (seconds < 0 || seconds > 1800)
        return;
    timer_.stop();
    QSettings().setValue(settingsKey_, seconds == 0);
    if (seconds > 0)
        timer_.start(seconds * 1000);
    if (!paused_) {
        paused_ = true;
        emit changed();
    }
}

void HistoryPause::resume()
{
    timer_.stop();
    QSettings().remove(settingsKey_);
    if (paused_) {
        paused_ = false;
        emit changed();
    }
}
