#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QQuickTextDocument>

class QQuickWindow;

class QuickNotes : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString defaultFolder READ defaultFolder NOTIFY defaultFolderChanged)
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit QuickNotes(QObject *parent = nullptr);
    QString defaultFolder() const;
    QVariantList notes() const { return notes_; }
    QString error() const { return error_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void chooseDefaultFolder();
    Q_INVOKABLE int formatInline(QQuickTextDocument *editor, int position, bool newLine);
    Q_INVOKABLE int restoreInline(QQuickTextDocument *editor, int position);
    Q_INVOKABLE QString markdownText(QQuickTextDocument *editor) const;
    Q_INVOKABLE QString read(const QString &path);
    Q_INVOKABLE bool save(const QString &markdown);
    Q_INVOKABLE bool saveAs(const QString &markdown, QQuickWindow *parent);

signals:
    void defaultFolderChanged();
    void notesChanged();
    void errorChanged();

private:
    void rememberKnownNotes();
    void setError(const QString &error);
    bool write(const QString &path, const QString &markdown);

    QVariantList notes_;
    QString error_;
    QSettings settings_;
};
