#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QString>
#include "aiclient.h"

class QNetworkAccessManager;
class QNetworkReply;

class ClaudeClient : public AiClient
{
    Q_OBJECT

public:
    explicit ClaudeClient(QObject *parent = nullptr);
    ~ClaudeClient();

    void sendMessage(const QString &userMessage) override;

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_network;
    QString m_lastMessage;
    int     m_retryCount = 0;

    void doSend(const QString &userMessage);
};

#endif // CLAUDECLIENT_H
