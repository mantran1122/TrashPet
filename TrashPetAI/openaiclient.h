#ifndef OPENAICLIENT_H
#define OPENAICLIENT_H

#include <QString>
#include "aiclient.h"

class QNetworkAccessManager;
class QNetworkReply;

class OpenAiClient : public AiClient
{
    Q_OBJECT

public:
    explicit OpenAiClient(QObject *parent = nullptr);
    ~OpenAiClient();

    void sendMessage(const QString &userMessage) override;

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_network;
    QString m_lastMessage;
    int     m_retryCount = 0;

    void doSend(const QString &userMessage);
};

#endif // OPENAICLIENT_H
