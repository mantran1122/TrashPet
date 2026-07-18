#ifndef GEMINICLIENT_H
#define GEMINICLIENT_H

#include <QObject>
#include <QString>
#include "aiclient.h"

class QNetworkAccessManager;
class QNetworkReply;

class GeminiClient : public AiClient
{
    Q_OBJECT

public:
    explicit GeminiClient(QObject *parent = nullptr);
    ~GeminiClient();

    // Gửi câu hỏi tới Gemini
    void sendMessage(const QString &userMessage) override;

private slots:
    // Xử lý khi HTTP request hoàn tất
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_network;
    QString m_lastMessage;  // Lưu để retry
    int     m_retryCount = 0;

    void doSend(const QString &userMessage);
};

#endif // GEMINICLIENT_H