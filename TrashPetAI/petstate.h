#ifndef PETSTATE_H
#define PETSTATE_H

#include <QString>

class PetState {
public:
    static PetState& instance();

    bool load(const QString &filePath);
    bool save() const;

    int    hunger()          const { return m_hunger; }
    int    petSize()         const { return m_petSize; }
    qint64 lifetimeEatenMb() const { return m_lifetimeEatenMb; }
    QString costume()        const { return m_costume; }
    QString aiProvider()     const { return m_aiProvider; }  // "" = dùng mặc định trong config.json

    void setHunger(int h);
    void setPetSize(int s);
    void addEatenMb(qint64 mb) { m_lifetimeEatenMb += mb; }
    void setCostume(const QString &id) { m_costume = id; }
    void setAiProvider(const QString &id) { m_aiProvider = id; }

    QString lastError() const { return m_lastError; }

private:
    PetState() = default;
    PetState(const PetState&) = delete;
    PetState& operator=(const PetState&) = delete;

    int     m_hunger          = 0;
    int     m_petSize         = 160;
    qint64  m_lifetimeEatenMb = 0;
    QString m_costume;          // "" = trang phục mặc định (slime)
    QString m_aiProvider;       // "" = dùng ai_provider trong config.json
    QString m_savePath;
    QString m_lastError;
};

#endif // PETSTATE_H
