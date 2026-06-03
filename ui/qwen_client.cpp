#include "qwen_client.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcessEnvironment>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

QwenClient::QwenClient(QObject *parent) : QObject(parent)
{
    connect(&nam_, &QNetworkAccessManager::finished, this, &QwenClient::onReplyFinished);
}

void QwenClient::setApiKey(const QString &apiKey)
{
    apiKey_ = apiKey;
}

void QwenClient::setModel(const QString &model)
{
    model_ = model;
}

void QwenClient::setBaseUrl(const QString &baseUrl)
{
    baseUrl_ = baseUrl;
}

void QwenClient::chat(const QList<Message> &messages)
{
    if (apiKey_.trimmed().isEmpty())
    {
        const QString envKey = QProcessEnvironment::systemEnvironment().value("DASHSCOPE_API_KEY").trimmed();
        if (!envKey.isEmpty())
        {
            apiKey_ = envKey;
        }
        else
        {
            emit errorOccurred("未读取到 DASHSCOPE_API_KEY。请确认已在“启动程序的那个终端/IDE”的运行环境里配置，并重启程序后再试。");
            return;
        }
    }

    QJsonArray msgArr;
    for (const Message &m : messages)
    {
        QJsonObject obj;
        obj["role"] = m.role;
        obj["content"] = m.content;
        msgArr.append(obj);
    }

    QJsonObject body;
    body["model"] = model_;
    body["messages"] = msgArr;
    body["temperature"] = 0.7;

    const QUrl url(baseUrl_ + "/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey_.toUtf8());

    nam_.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void QwenClient::onReplyFinished(QNetworkReply *reply)
{
    const QByteArray raw = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString err = reply->errorString();
    const bool ok = (reply->error() == QNetworkReply::NoError) && (status >= 200 && status < 300);

    reply->deleteLater();

    if (!ok)
    {
        const QString detail = QString("HTTP %1：%2").arg(status).arg(err);
        emit errorOccurred(detail);
        return;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        emit errorOccurred("解析千问响应失败。");
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty())
    {
        emit errorOccurred("千问响应缺少 choices。");
        return;
    }

    const QJsonObject choice0 = choices.at(0).toObject();
    const QJsonObject msg = choice0.value("message").toObject();
    const QString content = msg.value("content").toString();
    if (content.trimmed().isEmpty())
    {
        emit errorOccurred("千问返回内容为空。");
        return;
    }

    emit responseReady(content);
}
