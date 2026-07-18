#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>

class ConfigManager
{
public:
    // Singleton pattern: chỉ có 1 instance trong cả app
    static ConfigManager& instance();

    // Đọc config từ file. Trả về true nếu OK.
    bool load(const QString &filePath);

    // Tạo file config mặc định (không có API key nào) tại đường dẫn chỉ định.
    static bool writeDefaultConfig(const QString &filePath);

    // Ghi API key + provider vào file config (giữ nguyên các field khác), rồi load lại.
    bool saveProviderApiKey(const QString &filePath, const QString &provider, const QString &key);

    // Provider đang chọn đã có API key hợp lệ chưa.
    bool hasValidApiKeyForCurrentProvider() const;

    QString configFilePath() const { return m_configFilePath; }

    // Getters
    QString apiKey()        const { return m_apiKey; }
    QString model()         const { return m_model; }
    QString characterName() const { return m_characterName; }
    QString language()      const { return m_language; }
    bool    alwaysOnTop()   const { return m_alwaysOnTop; }
    bool    confirmEmptyTrash() const { return m_confirmEmptyTrash; }

    // Multi-provider AI
    QString aiProvider()    const { return m_aiProvider; }   // "gemini" | "openai" | "claude"
    QString openaiApiKey()  const { return m_openaiApiKey; }
    QString openaiModel()   const { return m_openaiModel; }
    QString claudeApiKey()  const { return m_claudeApiKey; }
    QString claudeModel()   const { return m_claudeModel; }

    // Báo có lỗi không
    QString lastError() const { return m_lastError; }

private:
    ConfigManager() = default;  // Private constructor (singleton)
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString m_apiKey;
    QString m_model = "gemini-3.5-flash";
    QString m_characterName = "TrashPet";
    QString m_language = "vi";
    bool    m_alwaysOnTop = true;
    bool    m_confirmEmptyTrash = true;

    QString m_aiProvider   = "gemini";
    QString m_openaiApiKey;
    QString m_openaiModel  = "gpt-4o-mini";
    QString m_claudeApiKey;
    QString m_claudeModel  = "claude-opus-4-8";

    QString m_configFilePath;
    QString m_lastError;
};

#endif // CONFIGMANAGER_H