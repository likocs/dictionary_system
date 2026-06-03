#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

class QNetworkReply;

class QwenClient final : public QObject
{
    Q_OBJECT

public:
    struct Message
    {
        QString role;
        QString content;
    };

    explicit QwenClient(QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    void setModel(const QString &model);
    void setBaseUrl(const QString &baseUrl);

    void chat(const QList<Message> &messages);

signals:
    void responseReady(const QString &text);
    void errorOccurred(const QString &message);

private:
    void onReplyFinished(QNetworkReply *reply);

    QNetworkAccessManager nam_;
    QString apiKey_;
    QString model_ = "qwen-turbo";
    QString baseUrl_ = "https://dashscope.aliyuncs.com/compatible-mode/v1";
};

