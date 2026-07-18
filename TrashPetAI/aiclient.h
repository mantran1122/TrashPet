#ifndef AICLIENT_H
#define AICLIENT_H

#include <QObject>
#include <QString>

// Interface chung cho mọi AI provider (Gemini / ChatGPT / Claude).
// Mỗi provider xử lý request/response khác nhau nhưng expose cùng 1 API.
class AiClient : public QObject
{
    Q_OBJECT

public:
    explicit AiClient(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~AiClient() = default;

    virtual void sendMessage(const QString &userMessage) = 0;

signals:
    void responseReceived(const QString &text);
    void errorOccurred(const QString &errorMessage);
};

#endif // AICLIENT_H
