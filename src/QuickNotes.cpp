#include "QuickNotes.h"
#include "AppConfig.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLocale>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextCursor>
#include <algorithm>

namespace {
constexpr auto savedPathsKey = "notes/savedPaths";
constexpr auto defaultFolderKey = "notes/defaultFolder";
constexpr int markdownDelimiter = QTextFormat::UserProperty;

QString dateHeading(const QDate &date)
{
    const int day = date.day();
    QString suffix = "th";
    if (day < 11 || day > 13) {
        switch (day % 10) {
        case 1: suffix = "st"; break;
        case 2: suffix = "nd"; break;
        case 3: suffix = "rd"; break;
        }
    }
    const QLocale english(QLocale::English);
    return english.toString(date, "dddd, d") + suffix + english.toString(date, " MMMM yyyy");
}

QDateTime savedDate(const QFileInfo &file)
{
    const QDate date = QDate::fromString(file.dir().dirName(), Qt::ISODate);
    const QString name = file.completeBaseName();
    const QTime time = QTime::fromString(name.left(8), "HH-mm-ss");
    if (date.isValid() && time.isValid()) {
        if (name.size() == 8)
            return QDateTime(date, time);
        if (name.at(8) == '-') {
            bool ok = false;
            const int millisecond = name.sliced(9).toInt(&ok);
            if (ok && millisecond >= 0 && millisecond < 1000)
                return QDateTime(date, time.addMSecs(millisecond));
        }
    }
    const QDateTime created = file.birthTime();
    return created.isValid() ? created : file.lastModified();
}

}

QuickNotes::QuickNotes(QObject *parent) : QObject(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, [this] {
        rememberKnownNotes();
        emit defaultFolderChanged();
        refresh();
    });
    refresh();
}

QString QuickNotes::defaultFolder() const
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString folder = QDir(documents.isEmpty() ? QDir::homePath() : documents).filePath("Clipify Notes");
    return AppConfig::instance().value(defaultFolderKey, folder).toString();
}

void QuickNotes::chooseDefaultFolder()
{
    const QString current = defaultFolder();
    const QString folder = QFileDialog::getExistingDirectory(nullptr, tr("Choose notes folder"),
                                  QDir(current).exists() ? current : QDir::homePath());
    if (folder.isEmpty() || folder == current)
        return;

    rememberKnownNotes();
    if (!AppConfig::instance().setValue(defaultFolderKey, folder))
        return;
    emit defaultFolderChanged();
    refresh();
}

void QuickNotes::rememberKnownNotes()
{
    auto paths = settings_.value(savedPathsKey).toStringList();
    for (const auto &note : notes_) {
        const QString path = note.toMap().value("path").toString();
        if (!paths.contains(path))
            paths.append(path);
    }
    settings_.setValue(savedPathsKey, paths);
}

void QuickNotes::refresh()
{
    QSet<QString> paths;
    const auto collect = [&paths](const QDir &folder) {
        const auto local = folder.entryInfoList({"*.md", "*.markdown"}, QDir::Files | QDir::Readable);
        for (const auto &file : local)
            paths.insert(file.absoluteFilePath());
    };
    const QDir root(defaultFolder());
    collect(root);
    const auto folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &folder : folders) {
        if (QDate::fromString(folder, Qt::ISODate).isValid())
            collect(QDir(root.filePath(folder)));
    }

    const auto savedPaths = settings_.value(savedPathsKey).toStringList();
    for (const auto &path : savedPaths) {
        if (QFileInfo(path).isFile())
            paths.insert(path);
    }

    QList<QFileInfo> files;
    for (const auto &path : paths)
        files.append(QFileInfo(path));
    std::sort(files.begin(), files.end(), [](const QFileInfo &left, const QFileInfo &right) {
        const QDateTime leftDate = savedDate(left);
        const QDateTime rightDate = savedDate(right);
        return leftDate == rightDate ? left.filePath() < right.filePath() : leftDate > rightDate;
    });

    QVariantList notes;
    for (const auto &file : files) {
        const QDateTime date = savedDate(file).toLocalTime();
        notes.append(QVariantMap{{"path", file.absoluteFilePath()},
                                 {"date", date.date().toString(Qt::ISODate)},
                                 {"dateHeading", dateHeading(date.date())},
                                 {"savedAt", date}});
    }
    if (notes_ != notes) {
        notes_ = std::move(notes);
        emit notesChanged();
    }
}

int QuickNotes::formatInline(QQuickTextDocument *editor, int position, bool newLine)
{
    auto *document = editor->textDocument();
    const QTextBlock block = document->findBlock(position);
    if (!block.isValid())
        return -1;

    static const QRegularExpression inlineMarkup(
        R"((?:^|\s)(\*\*\*|\*\*|__|~~|`|\*|_)(\S(?:[^*_`~]*?\S)?)\1$)");
    const auto match = inlineMarkup.match(block.text().left(position - block.position()));
    if (!match.hasMatch())
        return -1;

    const QString delimiter = match.captured(1);
    const QString content = match.captured(2);
    QTextCharFormat format;
    format.setProperty(markdownDelimiter, delimiter);
    if (delimiter == "**" || delimiter == "__" || delimiter == "***")
        format.setFontWeight(QFont::Bold);
    if (delimiter == "*" || delimiter == "_" || delimiter == "***")
        format.setFontItalic(true);
    if (delimiter == "~~")
        format.setFontStrikeOut(true);
    if (delimiter == "`")
        format.setFontFamilies({"monospace"});

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.setPosition(block.position() + match.capturedStart(1));
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    cursor.insertText(content, format);
    if (newLine)
        cursor.insertBlock(cursor.blockFormat(), QTextCharFormat());
    else
        cursor.insertText(" ", QTextCharFormat());
    cursor.endEditBlock();
    return cursor.position();
}

int QuickNotes::restoreInline(QQuickTextDocument *editor, int position)
{
    auto *document = editor->textDocument();
    int end = position;
    if (end > 0 && document->characterAt(end - 1).isSpace())
        --end;
    if (end <= 0)
        return -1;

    const QTextBlock block = document->findBlock(end - 1);
    int start = block.position();
    QString delimiter;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        const QString marker = fragment.charFormat().stringProperty(markdownDelimiter);
        if (fragment.position() >= end) {
            if (marker == delimiter)
                return -1;
            break;
        }
        if (fragment.position() + fragment.length() > end)
            return -1;
        if (marker != delimiter) {
            delimiter = marker;
            start = fragment.position();
        }
    }
    if (delimiter.isEmpty())
        return -1;

    QTextCursor cursor(document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    const QString source = delimiter + cursor.selectedText() + delimiter;
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    cursor.insertText(source, QTextCharFormat());
    return cursor.position();
}

QString QuickNotes::markdownText(QQuickTextDocument *editor) const
{
    QStringList lines;
    for (auto block = editor->textDocument()->begin(); block.isValid(); block = block.next()) {
        QString line;
        QString activeDelimiter;
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            const QTextFragment text = fragment.fragment();
            const QString delimiter = text.charFormat().stringProperty(markdownDelimiter);
            if (delimiter != activeDelimiter) {
                line += activeDelimiter + delimiter;
                activeDelimiter = delimiter;
            }
            line += text.text();
        }
        lines.append(line + activeDelimiter);
    }
    return lines.join('\n');
}

QString QuickNotes::read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Could not open the note: %1").arg(file.errorString()));
        return {};
    }

    setError({});
    return QString::fromUtf8(file.readAll());
}

bool QuickNotes::save(const QString &markdown)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDir folder(QDir(defaultFolder()).filePath(now.date().toString(Qt::ISODate)));
    if (!QDir().mkpath(folder.absolutePath())) {
        setError(tr("Could not create the notes folder."));
        return false;
    }

    const QString name = now.time().toString("HH-mm-ss");
    QString path = folder.filePath(name + ".md");
    for (int millisecond = now.time().msec(); QFileInfo::exists(path); ++millisecond)
        path = folder.filePath(QStringLiteral("%1-%2.md").arg(name).arg(millisecond, 3, 10, QLatin1Char('0')));
    return write(path, markdown);
}

bool QuickNotes::saveAs(const QString &markdown, QQuickWindow *parent)
{
    setError({});
    const QDateTime now = QDateTime::currentDateTime();
    const QDir root(defaultFolder());
    const QDir datedFolder(root.filePath(now.date().toString(Qt::ISODate)));
    const QDir startFolder = datedFolder.exists() ? datedFolder : root.exists() ? root : QDir::home();
    QFileDialog dialog(nullptr, tr("Save note"), startFolder.filePath(now.time().toString("HH-mm-ss") + ".md"),
                       tr("Markdown files (*.md *.markdown)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("md");
    if (parent) {
        dialog.winId();
        dialog.windowHandle()->setTransientParent(parent);
    }
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString path = dialog.selectedFiles().first();
    if (!write(path, markdown))
        return false;

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (QFileInfo(path).absolutePath() != defaultFolder() || (suffix != "md" && suffix != "markdown")) {
        auto paths = settings_.value(savedPathsKey).toStringList();
        if (!paths.contains(path)) {
            paths.append(path);
            settings_.setValue(savedPathsKey, paths);
            refresh();
        }
    }
    return true;
}

bool QuickNotes::write(const QString &path, const QString &markdown)
{
    QSaveFile file(path);
    const QByteArray bytes = markdown.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(tr("Could not save the note: %1").arg(file.errorString()));
        return false;
    }

    setError({});
    refresh();
    return true;
}

void QuickNotes::setError(const QString &error)
{
    if (error_ != error) {
        error_ = error;
        emit errorChanged();
    }
}
