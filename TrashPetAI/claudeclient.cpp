#include "claudeclient.h"
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
constexpr int kMaxTokens = 1024;

// Cùng cá tính TrashPet như bên Gemini, để trải nghiệm nhất quán dù đổi AI provider.
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

ClaudeClient::ClaudeClient(QObject *parent)
    : AiClient(parent)
{
    m_network = new QNetworkAccessManager(this);
    connect(m_network, &QNetworkAccessManager::finished,
            this, &ClaudeClient::onReplyFinished);
}

ClaudeClient::~ClaudeClient() {}

void ClaudeClient::sendMessage(const QString &userMessage)
{
    m_lastMessage = userMessage;
    m_retryCount = 0;
    doSend(userMessage);
}

void ClaudeClient::doSend(const QString &userMessage)
{
    const ConfigManager &cfg = ConfigManager::instance();

    if (cfg.claudeApiKey().isEmpty()) {
        emit errorOccurred("Chưa cấu hình claude_api_key trong config.json");
        return;
    }

    QNetworkRequest request{QUrl("https://api.anthropic.com/v1/messages")};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", cfg.claudeApiKey().toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = userMessage;
    QJsonArray messages;
    messages.append(userMsg);

    QJsonObject root;
    root["model"]      = cfg.claudeModel();
    root["max_tokens"] = kMaxTokens;
    root["system"]     = kSystemPrompt;
    root["messages"]   = messages;

    QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
    m_network->post(request, body);
}

void ClaudeClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    int httpStatus = reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QByteArray data = reply->readAll();  // đọc body TRƯỚC — Qt vẫn trả về body kể cả khi lỗi HTTP (401/400/...)

    if (reply->error() != QNetworkReply::NoError) {
        // 429 rate limit hoặc 529 overloaded -> retry tối đa 3 lần
        if ((httpStatus == 429 || httpStatus == 529 || httpStatus >= 500) && m_retryCount < 3) {
            m_retryCount++;
            QTimer::singleShot(2000, this, [this]() {
                doSend(m_lastMessage);
            });
            return;
        }

        // Thử đọc message lỗi thật từ body JSON của Anthropic thay vì
        // dùng errorString() chung chung của Qt (vd: 401 -> "Host requires authentication").
        QJsonDocument errDoc = QJsonDocument::fromJson(data);
        if (errDoc.isObject() && errDoc.object().contains("error")) {
            QString msg = errDoc.object().value("error").toObject().value("message").toString();
            const QString errType = errDoc.object().value("error").toObject().value("type").toString();
            if (errType == "authentication_error" || httpStatus == 401) {
                emit errorOccurred("Claude API key không hợp lệ. Kiểm tra lại key trong ⚙️ Cài đặt: " + msg);
            } else {
                emit errorOccurred("Lỗi API Claude: " + msg);
            }
            return;
        }

        QString detail = (httpStatus == 529)
                             ? "Server Claude đang quá tải, thử lại sau vài phút nhé!"
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
        emit errorOccurred("Lỗi API Claude: " + msg);
        return;
    }

    const QString stopReason = obj.value("stop_reason").toString();
    if (stopReason == "refusal") {
        emit errorOccurred("Claude từ chối trả lời câu này (chính sách an toàn).");
        return;
    }

    QJsonArray content = obj.value("content").toArray();
    if (content.isEmpty()) {
        emit errorOccurred("Claude không trả lời (content rỗng)");
        return;
    }

    QString text;
    for (const auto &blockVal : content) {
        QJsonObject block = blockVal.toObject();
        if (block.value("type").toString() == "text") {
            text += block.value("text").toString();
        }
    }

    if (text.isEmpty()) {
        emit errorOccurred("Câu trả lời rỗng");
        return;
    }

    emit responseReceived(text);
}
