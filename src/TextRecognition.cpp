#include "TextRecognition.h"
#include "AppConfig.h"

#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QRect>
#include <QStandardPaths>
#include <algorithm>

namespace {
QString recognizedParagraphs(const QByteArray &tsv)
{
    QString text;
    QString line;
    QRect bounds;
    QRect previous;
    const auto appendLine = [&] {
        if (line.isEmpty())
            return;

        if (!previous.isNull()) {
            const int gap = bounds.top() - previous.bottom() - 1;
            const bool separate = bounds.top() <= previous.top()
                               || bounds.left() > previous.right()
                               || bounds.right() < previous.left()
                               || gap >= std::max(previous.height(), bounds.height());
            text += separate ? QStringLiteral("\n\n") : QStringLiteral(" ");
        }
        text += line;
        previous = bounds;
        line.clear();
    };

    for (const auto &record : tsv.split('\n')) {
        if (!record.startsWith("4\t") && !record.startsWith("5\t"))
            continue;

        const auto fields = record.split('\t');
        if (fields.size() < 12)
            continue;

        if (fields[0] == "4") {
            appendLine();
            bounds = QRect(fields[6].toInt(), fields[7].toInt(),
                           fields[8].toInt(), fields[9].toInt());
        } else {
            const QString word = QString::fromUtf8(fields[11]).trimmed();
            if (word.isEmpty())
                continue;
            if (!line.isEmpty())
                line += ' ';
            line += word;
        }
    }
    appendLine();
    return text;
}
}

TextRecognition::TextRecognition(QObject *parent) : QObject(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, &TextRecognition::accurateChanged);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setStatus(tr("Install Tesseract to recognize text."));
    });
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exitStatus) {
        const QString text = recognizedParagraphs(process_.readAllStandardOutput());
        if (exitStatus != QProcess::NormalExit || code != 0) {
            setStatus(tr("Text recognition failed. Check that the German and English model files are installed."));
        } else if (text.isEmpty()) {
            setStatus(tr("No text found in the selection."));
        } else {
            QGuiApplication::clipboard()->setText(text);
            setStatus(tr("Recognized text copied."));
        }
    });
}

bool TextRecognition::accurate() const
{
    return AppConfig::instance().value("clipboard/accurateOcr", false).toBool();
}

void TextRecognition::setAccurate(bool accurate)
{
    if (accurate == this->accurate())
        return;
    if (!AppConfig::instance().setValue("clipboard/accurateOcr", accurate))
        return;
    setStatus({});
    emit accurateChanged();
}

void TextRecognition::recognize(const QImage &image)
{
    if (process_.state() != QProcess::NotRunning)
        return;

    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    auto program = QStandardPaths::findExecutable("tesseract");
    if (program.isEmpty())
        program = QStandardPaths::findExecutable("tesseract", {data.filePath("ocr")});
    if (program.isEmpty()) {
        setStatus(tr("Install Tesseract to recognize text."));
        return;
    }

    QStringList arguments{"stdin", "stdout", "-l", "deu+eng", "--oem", "1", "--psm", "3",
                          "-c", "tessedit_create_tsv=1"};
    if (accurate()) {
        const QDir models(data.filePath("ocr/tessdata_best"));
        if (!models.exists("deu.traineddata") || !models.exists("eng.traineddata")) {
            setStatus(tr("Install the tessdata_best models to use Accurate mode."));
            return;
        }
        arguments << "--tessdata-dir" << models.absolutePath();
    }
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        setStatus(tr("Could not prepare the selection for recognition."));
        return;
    }

    setStatus(tr("Recognizing text…"));
    process_.start(program, arguments);
    process_.write(png);
    process_.closeWriteChannel();
}

void TextRecognition::setStatus(const QString &status)
{
    if (status_ != status) {
        status_ = status;
        emit statusChanged();
    }
}
