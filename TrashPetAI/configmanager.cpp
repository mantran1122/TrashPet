#include "configmanager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager s_instance;
    return s_instance;
}

bool ConfigManager::load(const QString &filePath)
{
    m_configFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Không mở được file config: " + filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        m_lastError = "File config sai định dạng JSON: " + parseErr.errorString();
        return false;
    }

    QJsonObject obj = doc.object();

    m_apiKey            = obj.value("gemini_api_key").toString();
    m_model             = obj.value("gemini_model").toString("gemini-3.5-flash");
    m_characterName     = obj.value("character_name").toString("TrashPet");
    m_language          = obj.value("language").toString("vi");
    m_alwaysOnTop       = obj.value("always_on_top").toBool(true);
    m_confirmEmptyTrash = obj.value("confirm_before_empty_trash").toBool(true);

    m_aiProvider   = obj.value("ai_provider").toString("gemini");
    m_openaiApiKey = obj.value("openai_api_key").toString();
    m_openaiModel  = obj.value("openai_model").toString("gpt-4o-mini");
    m_claudeApiKey = obj.value("claude_api_key").toString();
    m_claudeModel  = obj.value("claude_model").toString("claude-opus-4-8");

    if (!hasValidApiKeyForCurrentProvider()) {
        m_lastError = "Chưa cấu hình API key cho provider \"" + m_aiProvider + "\" trong config.json";
        return false;
    }

    return true;
}

bool ConfigManager::hasValidApiKeyForCurrentProvider() const
{
    if (m_aiProvider == "openai")
        return !m_openaiApiKey.isEmpty() && m_openaiApiKey != "YOUR_OPENAI_API_KEY_HERE";
    if (m_aiProvider == "claude")
        return !m_claudeApiKey.isEmpty() && m_claudeApiKey != "YOUR_CLAUDE_API_KEY_HERE";
    return !m_apiKey.isEmpty() && m_apiKey != "YOUR_API_KEY_HERE";
}

bool ConfigManager::writeDefaultConfig(const QString &filePath)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QJsonObject obj;
    obj["ai_provider"]              = "gemini";
    obj["gemini_api_key"]           = "";
    obj["gemini_model"]             = "gemini-3.5-flash";
    obj["openai_api_key"]           = "";
    obj["openai_model"]             = "gpt-4o-mini";
    obj["claude_api_key"]           = "";
    obj["claude_model"]             = "claude-opus-4-8";
    obj["character_name"]           = "TrashPet";
    obj["always_on_top"]            = true;
    obj["confirm_before_empty_trash"] = true;
    obj["language"]                 = "vi";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(obj).toJson());
    return true;
}

bool ConfigManager::saveProviderApiKey(const QString &filePath, const QString &provider, const QString &key)
{
    // Đọc config hiện tại (nếu có) để không mất các key provider khác đã cấu hình.
    QJsonObject obj;
    QFile readFile(filePath);
    if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(readFile.readAll());
        readFile.close();
        if (doc.isObject()) obj = doc.object();
    }

    obj["ai_provider"] = provider;
    if (provider == "openai") {
        obj["openai_api_key"] = key;
    } else if (provider == "claude") {
        obj["claude_api_key"] = key;
    } else {
        obj["gemini_api_key"] = key;
    }

    // Đảm bảo các field còn thiếu có giá trị mặc định hợp lý.
    if (!obj.contains("gemini_model"))  obj["gemini_model"]  = "gemini-3.5-flash";
    if (!obj.contains("openai_model"))  obj["openai_model"]  = "gpt-4o-mini";
    if (!obj.contains("claude_model"))  obj["claude_model"]  = "claude-opus-4-8";
    if (!obj.contains("character_name")) obj["character_name"] = "TrashPet";
    if (!obj.contains("always_on_top")) obj["always_on_top"] = true;
    if (!obj.contains("confirm_before_empty_trash")) obj["confirm_before_empty_trash"] = true;
    if (!obj.contains("language")) obj["language"] = "vi";

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile writeFile(filePath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = "Không ghi được file config: " + filePath;
        return false;
    }
    writeFile.write(QJsonDocument(obj).toJson());
    writeFile.close();

    return load(filePath);
}
