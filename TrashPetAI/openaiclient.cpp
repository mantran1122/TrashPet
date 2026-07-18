#include "openaiclient.h"
#include "configmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>

namespace {
// Cùng cá tính TrashPet như bên Gemini/Claude, để trải nghiệm nhất quán dù đổi AI provider.
const QString kSystemPrompt = QStringLiteral(
    "Bạn là TrashPet, một chú slime AI dễ thương sống trên desktop của người dùng. "
    "Tính cách: vui vẻ, thân thiện, hơi nhõng nhẽo, thích ăn rác (file trong Recycle Bin). "
    "Quy tắc trả lời: "
    "- Xưng \"mình\", gọi người dùng là \"bạn\". "
    "- Luôn trả lời bằng tiếng Việt trừ khi người dùng yêu cầu ngôn ngữ khác. "
    "- Trả lời ngắn gọn, tự nhiên như đang nhắn tin (2-4 câu là vừa, trừ khi được hỏi chi tiết). "
    "- Thỉnh thoảng dùng emoji nhẹ nhàng (🐌 ✨ 🟢) nhưng không lạm dụng. "
    "- KHÔNG dùng markdown (không **bold**, không ## heading, không bullet *). "
    "- Nếu người dùng hỏi về thùng rác/Recycle Bin, có thể nhắc rằng bạn có thể \"ăn\" giúp họ.");
}

OpenAiClient::OpenAiClient(QObject *parent)
    : AiClient(parent)
{
    m_network = new QNetworkAccessManager(this);
    connect(m_network, &QNetworkAccessManager::finished,
            this, &OpenAiClient::onReplyFinished);
}

OpenAiClient::~OpenAiClient() {}

void OpenAiClient::sendMessage(const QString &userMessage)
{
    m_lastMessage = userMessage;
    m_retryCount = 0;
    doSend(userMessage);
}

void OpenAiClient::doSend(const QString &userMessage)
{
    const ConfigManager &cfg = ConfigManager::instance();

    if (cfg.openaiApiKey().isEmpty()) {
        emit errorOccurred("Chưa cấu hình openai_api_key trong config.json");
        return;
    }

    QNetworkRequest request{QUrl("https://api.openai.com/v1/chat/completions")};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + cfg.openaiApiKey()).toUtf8());

    QJsonObject sysMsg;
    sysMsg["role"]    = "system";
    sysMsg["content"] = kSystemPrompt;

    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = userMessage;

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject root;
    root["model"]    = cfg.openaiModel();
    root["messages"] = messages;

    QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
    m_network->post(request, body);
}

void OpenAiClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    int httpStatus = reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QByteArray data = reply->readAll();  // đọc body TRƯỚC — Qt vẫn trả về body kể cả khi lỗi HTTP (401/400/...)

    if (reply->error() != QNetworkReply::NoError) {
        // 429 rate limit hoặc 5xx server -> retry tối đa 3 lần
        if ((httpStatus == 429 || httpStatus >= 500) && m_retryCount < 3) {
            m_retryCount++;
            QTimer::singleShot(2000, this, [this]() {
                doSend(m_lastMessage);
            });
            return;
        }

        // Thử đọc message lỗi thật từ body JSON của OpenAI thay vì
        // dùng errorString() chung chung của Qt (vd: 401 -> "Host requires authentication").
        QJsonDocument errDoc = QJsonDocument::fromJson(data);
        if (errDoc.isObject() && errDoc.object().contains("error")) {
            QString msg = errDoc.object().value("error").toObject().value("message").toString();
            if (httpStatus == 401) {
                emit errorOccurred("ChatGPT API key không hợp lệ. Kiểm tra lại key trong ⚙️ Cài đặt: " + msg);
            } else {
                emit errorOccurred("Lỗi API ChatGPT: " + msg);
            }
            return;
        }

        QString detail = (httpStatus == 429)
                             ? "ChatGPT đang bị giới hạn tốc độ, thử lại sau vài phút nhé!"
                             : (httpStatus == 401)
                                 ? "API key không hợp lệ hoặc chưa cấu hình đúng."
                                 : reply->errorString();
        emit errorOccurred("Lỗi mạng: " + detail);
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred("Phản hồi không phải JSON hợp lệ");
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        QString msg = obj.value("error").toObject().value("message").toString();
        emit errorOccurred("Lỗi API ChatGPT: " + msg);
        return;
    }

    QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty()) {
        emit errorOccurred("ChatGPT không trả lời (choices rỗng)");
        return;
    }

    QJsonObject message = choices.first().toObject().value("message").toObject();
    QString text = message.value("content").toString();

    if (text.isEmpty()) {
        emit errorOccurred("Câu trả lời rỗng");
        return;
    }

    emit responseReceived(text);
}
