#include "LlmService.h"
#include "AppConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>

namespace {
constexpr auto cloudKey = "llm/cloud";
constexpr auto providerKey = "llm/provider";

QString polishModelKey(const QString &provider)
{
    return "llm/polishModel/" + provider;
}

QString credentialKey(const QString &provider)
{
    return "llmCredentials/" + provider;
}

struct Provider {
    QString id;
    QUrl endpoint;
    QString tokenLimit;
    QUrl modelsEndpoint;
};

const Provider providers[] = {
    {"openrouter", QUrl("https://openrouter.ai/api/v1/chat/completions"), "max_completion_tokens",
     QUrl("https://openrouter.ai/api/v1/models/user")},
    {"openai", QUrl("https://api.openai.com/v1/chat/completions"), "max_completion_tokens",
     QUrl("https://api.openai.com/v1/models")},
    {"anthropic", QUrl("https://api.anthropic.com/v1/messages"), "max_tokens", QUrl("https://api.anthropic.com/v1/models")}
};

const Provider *findProvider(const QString &id)
{
    for (const auto &provider : providers) {
        if (provider.id == id)
            return &provider;
    }
    return nullptr;
}

QNetworkRequest authenticatedRequest(const Provider &provider, const QUrl &url, const QString &key)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (provider.id == "anthropic") {
        request.setRawHeader("x-api-key", key.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    }
    return request;
}

void setDeadline(QNetworkReply *reply)
{
    QTimer::singleShot(15000, reply, [reply] {
        if (!reply->isFinished()) {
            reply->setProperty("llmTimedOut", true);
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

QString readCompletion(const QJsonObject &response, bool anthropic)
{
    if (anthropic) {
        if (response.value("stop_reason").toString() != "end_turn")
            return {};
        QString result;
        for (const auto &part : response.value("content").toArray()) {
            const auto block = part.toObject();
            if (block.value("type").toString() == "text")
                result += block.value("text").toString();
        }
        return result.trimmed();
    }

    const auto choices = response.value("choices").toArray();
    if (choices.isEmpty())
        return {};
    const auto choice = choices.first().toObject();
    if (choice.value("finish_reason").toString() != "stop")
        return {};
    return choice.value("message").toObject().value("content").toString().trimmed();
}
}

LlmService::LlmService(QObject *parent) : QObject(parent)
{
    connect(&AppConfig::instance(), &AppConfig::reloaded, this, [this](bool factoryReset) {
        resetCatalog();
        if (factoryReset) {
            QSettings settings;
            settings.remove("llmCredentials");
            settings.sync();
            if (settings.status() != QSettings::NoError)
                settingsError_ = tr("Could not remove the saved API keys.");
        }
        emit settingsChanged();
    });
}

bool LlmService::cloud() const
{
    return AppConfig::instance().value(cloudKey, false).toBool();
}

void LlmService::setCloud(bool value)
{
    if (value != cloud() && AppConfig::instance().setValue(cloudKey, value))
        resetCatalog();
    emit settingsChanged();
}

QString LlmService::provider() const
{
    const auto id = AppConfig::instance().value(providerKey).toString();
    return findProvider(id) ? id : QStringLiteral("openrouter");
}

void LlmService::setProvider(const QString &value)
{
    if (!findProvider(value))
        return;
    if (value != provider() && AppConfig::instance().setValue(providerKey, value))
        resetCatalog();
    emit settingsChanged();
}

QString LlmService::polishModel() const
{
    const auto &config = AppConfig::instance();
    return config.value("whisper/polishText").toBool()
        ? config.value(polishModelKey(provider())).toString() : QString();
}

void LlmService::setPolishModel(const QString &value)
{
    const QString model = value.trimmed();
    AppConfig::instance().setValues({{polishModelKey(provider()), model},
                                    {"whisper/polishText", !model.isEmpty()}});
    emit settingsChanged();
}

QString LlmService::apiKey() const
{
    return QSettings().value(credentialKey(provider())).toString();
}

QString LlmService::maskedApiKey() const
{
    const QString key = apiKey();
    if (key.isEmpty())
        return {};
    return (key.size() > 4 ? key.left(4) : QString()) + QStringLiteral("********");
}

bool LlmService::hasApiKey() const
{
    return !apiKey().isEmpty();
}

void LlmService::resetCatalog()
{
    auto *reply = catalogReply_;
    catalogReply_ = nullptr;
    if (reply)
        reply->abort();
    checkingApiKey_ = false;
    models_.clear();
    modelError_.clear();
    settingsError_.clear();
}

void LlmService::refreshModels()
{
    const QString key = apiKey();
    if (!cloud() || key.isEmpty() || loadingModels())
        return;
    modelError_.clear();
    fetchModels(key);
    emit settingsChanged();
}

void LlmService::clearKeyFeedback()
{
    if (settingsError_.isEmpty())
        return;
    settingsError_.clear();
    emit settingsChanged();
}

void LlmService::saveApiKey(const QString &value)
{
    if (!cloud() || checkingApiKey_)
        return;
    const QString key = value.trimmed();
    if (key.isEmpty() || key.contains('\r') || key.contains('\n')) {
        settingsError_ = tr("Enter a single API key.");
        emit settingsChanged();
        return;
    }

    if (auto *reply = catalogReply_) {
        catalogReply_ = nullptr;
        reply->abort();
    }
    settingsError_.clear();
    checkingApiKey_ = true;
    fetchModels(key);
    emit settingsChanged();
}

void LlmService::removeApiKey()
{
    resetCatalog();
    storeApiKey({});
    emit settingsChanged();
}

bool LlmService::storeApiKey(const QString &key)
{
    QSettings settings;
    const QString entry = credentialKey(provider());
    if (key.isEmpty())
        settings.remove(entry);
    else
        settings.setValue(entry, key);
    settings.sync();
    if (settings.status() == QSettings::NoError)
        return true;
    settingsError_ = key.isEmpty() ? tr("Could not remove the API key.") : tr("Could not save the API key.");
    return false;
}

void LlmService::fetchModels(const QString &key, QStringList models, const QString &cursor)
{
    const auto &selected = *findProvider(provider());
    QUrl url = selected.modelsEndpoint;
    if (!cursor.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("after_id", cursor);
        url.setQuery(query);
    }

    auto *reply = network_.get(authenticatedRequest(selected, url, key));
    catalogReply_ = reply;
    setDeadline(reply);
    const bool paginated = selected.id == "anthropic";
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, key, cursor, paginated, models = std::move(models)]() mutable {
        if (catalogReply_ != reply)
            return;
        catalogReply_ = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;
        if (status == 401 || status == 403)
            error = tr("The provider rejected this API key or its permissions.");
        else if (reply->property("llmTimedOut").toBool())
            error = tr("The provider did not respond in time.");
        else if (status && (status < 200 || status >= 300))
            error = tr("The provider returned HTTP %1.").arg(status);
        else if (reply->error() != QNetworkReply::NoError)
            error = tr("Could not reach the LLM provider.");

        if (error.isEmpty()) {
            const auto result = QJsonDocument::fromJson(reply->readAll()).object();
            const auto entries = result.value("data");
            if (!entries.isArray()) {
                error = tr("The provider returned an invalid model list.");
            } else {
                const auto page = entries.toArray();
                for (const auto &entry : page) {
                    const auto id = entry.toObject().value("id").toString();
                    if (!id.isEmpty())
                        models.append(id);
                }
                if (paginated && result.value("has_more").toBool()) {
                    const auto next = result.value("last_id").toString();
                    if (!next.isEmpty() && next != cursor) {
                        fetchModels(key, std::move(models), next);
                        return;
                    }
                    error = tr("The provider returned an invalid model list.");
                }
            }
        }

        const bool checking = checkingApiKey_;
        checkingApiKey_ = false;
        if (!error.isEmpty()) {
            if (checking)
                settingsError_ = error;
            else
                modelError_ = error;
            emit settingsChanged();
            return;
        }
        const bool saved = !checking || storeApiKey(key);
        if (saved) {
            models.removeDuplicates();
            std::sort(models.begin(), models.end());
            models_ = std::move(models);
            modelError_.clear();
        }
        emit settingsChanged();
        if (checking && saved)
            emit apiKeyVerified();
    });
}

QNetworkReply *LlmService::generateText(const QString &model, const QString &instructions,
                                       const QString &input, QObject *context, Completion completion)
{
    const auto *selected = findProvider(provider());
    const QString key = apiKey();
    if (!cloud() || !selected || key.isEmpty() || model.trimmed().isEmpty())
        return nullptr;

    QNetworkRequest request = authenticatedRequest(*selected, selected->endpoint, key);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const bool anthropic = selected->id == "anthropic";
    QJsonArray messages;
    QJsonObject payload{{"model", model}, {selected->tokenLimit, 4096}};
    if (anthropic) {
        payload.insert("system", instructions);
    } else {
        messages.append(QJsonObject{{"role", "system"}, {"content", instructions}});
        if (selected->id == "openai")
            payload.insert("store", false);
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", input}});
    payload.insert("messages", messages);

    auto *reply = network_.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    setDeadline(reply);
    connect(reply, &QNetworkReply::finished, context, [reply, anthropic, completion = std::move(completion)] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->property("llmTimedOut").toBool()) {
            completion({}, tr("The LLM request timed out."));
        } else if (status && (status < 200 || status >= 300)) {
            completion({}, tr("The LLM provider returned HTTP %1.").arg(status));
        } else if (reply->error() != QNetworkReply::NoError) {
            completion({}, tr("Could not reach the LLM provider."));
        } else {
            const auto response = QJsonDocument::fromJson(reply->readAll()).object();
            const QString text = readCompletion(response, anthropic);
            completion(text, text.isEmpty() ? tr("The LLM returned no complete text.") : QString());
        }
    });
    return reply;
}
