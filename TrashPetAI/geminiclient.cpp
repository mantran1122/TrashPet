#include "geminiclient.h"
#include "configmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>

GeminiClient::GeminiClient(QObject *parent)
    : AiClient(parent)
{
    m_network = new QNetworkAccessManager(this);

    connect(m_network, &QNetworkAccessManager::finished,
            this, &GeminiClient::onReplyFinished);
}

GeminiClient::~GeminiClient() {}

void GeminiClient::sendMessage(const QString &userMessage)
{
    m_lastMessage = userMessage;  // Lưu để retry
    m_retryCount = 0;
    doSend(userMessage);
}

void GeminiClient::doSend(const QString &userMessage)
{
    const ConfigManager &cfg = ConfigManager::instance();
    QString urlStr = QString(
                         "https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2"
                         ).arg(cfg.model(), cfg.apiKey());

    QNetworkRequest request{QUrl(urlStr)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // ===== System instruction: định nghĩa cá tính TrashPet =====
    const QString systemPrompt = QStringLiteral(
        "Bạn là TrashPet, một chú slime AI dễ thương sống trên desktop của người dùng. "
        "Tính cách: vui vẻ, thân thiện, hơi nhõng nhẽo, thích ăn rác (file trong Recycle Bin). "
        "Quy tắc trả lời: "
        "- Xưng \"mình\", gọi người dùng là \"bạn\". "
        "- Luôn trả lời bằng tiếng Việt trừ khi người dùng yêu cầu ngôn ngữ khác. "
        "- Trả lời ngắn gọn, tự nhiên như đang nhắn tin (2-4 câu là vừa, trừ khi được hỏi chi tiết). "
        "- Thỉnh thoảng dùng emoji nhẹ nhàng (🐌 ✨ 🟢) nhưng không lạm dụng. "
        "- KHÔNG dùng markdown (không **bold**, không ## heading, không bullet *). "
        "- Nếu người dùng hỏi về thùng rác/Recycle Bin, có thể nhắc rằng bạn có thể \"ăn\" giúp họ."
        );

    QJsonObject sysPart;
    sysPart["text"] = systemPrompt;
    QJsonArray sysParts;
    sysParts.append(sysPart);
    QJsonObject systemInstruction;
    systemInstruction["parts"] = sysParts;

    // ===== User message =====
    QJsonObject part;
    part["text"] = userMessage;
    QJsonArray parts;
    parts.append(part);
    QJsonObject content;
    content["parts"] = parts;
    QJsonArray contents;
    contents.append(content);

    // ===== Root JSON =====
    QJsonObject root;
    root["systemInstruction"] = systemInstruction;
    root["contents"]          = contents;

    QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
    m_network->post(request, body);
}
void GeminiClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    int httpStatus = reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        // Lỗi 503 (overload) hoặc 429 (rate limit) → retry tối đa 3 lần
        if ((httpStatus == 503 || httpStatus == 429) && m_retryCount < 3) {
            m_retryCount++;
            // Đợi 2 giây rồi thử lại
            QTimer::singleShot(2000, this, [this]() {
                doSend(m_lastMessage);
            });
            return;
        }

        QString detail = (httpStatus == 503)
                             ? "Server Gemini đang quá tải, thử lại sau vài phút nhé!"
                             : reply->errorString();
        emit errorOccurred("Lỗi mạng: " + detail);
        return;
    }

    QByteArray data = reply->readAll();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred("Phản hồi không phải JSON hợp lệ");
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        QString msg = obj.value("error").toObject().value("message").toString();
        emit errorOccurred("Lỗi API: " + msg);
        return;
    }

    QJsonArray candidates = obj.value("candidates").toArray();
    if (candidates.isEmpty()) {
        emit errorOccurred("Gemini không trả lời (candidates rỗng)");
        return;
    }

    QJsonObject firstCandidate = candidates.first().toObject();
    QJsonObject contentObj    = firstCandidate.value("content").toObject();
    QJsonArray  partsArr      = contentObj.value("parts").toArray();
    if (partsArr.isEmpty()) {
        emit errorOccurred("Câu trả lời không có nội dung");
        return;
    }

    QString text = partsArr.first().toObject().value("text").toString();

    if (text.isEmpty()) {
        emit errorOccurred("Câu trả lời rỗng");
        return;
    }

    emit responseReceived(text);
}