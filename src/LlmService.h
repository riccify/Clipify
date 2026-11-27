#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <functional>

class QNetworkReply;

class LlmService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool cloud READ cloud WRITE setCloud NOTIFY settingsChanged)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY settingsChanged)
    Q_PROPERTY(QString polishModel READ polishModel WRITE setPolishModel NOTIFY settingsChanged)
    Q_PROPERTY(QString maskedApiKey READ maskedApiKey NOTIFY settingsChanged)
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QStringList models READ models NOTIFY settingsChanged)
    Q_PROPERTY(bool loadingModels READ loadingModels NOTIFY settingsChanged)
    Q_PROPERTY(bool checkingApiKey READ checkingApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString modelError READ modelError NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsError READ settingsError NOTIFY settingsChanged)

public:
    explicit LlmService(QObject *parent = nullptr);

    bool cloud() const;
    void setCloud(bool value);
    QString provider() const;
    void setProvider(const QString &value);
    QString polishModel() const;
    void setPolishModel(const QString &value);
    QString maskedApiKey() const;
    bool hasApiKey() const;
    QString settingsError() const { return settingsError_; }
    QStringList models() const { return models_; }
    bool loadingModels() const { return catalogReply_ != nullptr; }
    bool checkingApiKey() const { return checkingApiKey_; }
    QString modelError() const { return modelError_; }
    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE void saveApiKey(const QString &value);
    Q_INVOKABLE void removeApiKey();
    Q_INVOKABLE void clearKeyFeedback();

    using Completion = std::function<void(const QString &text, const QString &error)>;
    QNetworkReply *generateText(const QString &model, const QString &instructions,
                                const QString &input, QObject *context, Completion completion);

signals:
    void settingsChanged();
    void apiKeyVerified();

private:
    QString apiKey() const;
    void resetCatalog();
    void fetchModels(const QString &key, QStringList models = {}, const QString &cursor = {});
    bool storeApiKey(const QString &key);

    QNetworkAccessManager network_;
    QNetworkReply *catalogReply_ = nullptr;
    QStringList models_;
    QString modelError_;
    QString settingsError_;
    bool checkingApiKey_ = false;
};
