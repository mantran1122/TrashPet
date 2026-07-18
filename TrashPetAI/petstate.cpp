#include "petstate.h"
#include "hungerconfig.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <algorithm>

PetState& PetState::instance() {
    static PetState s;
    return s;
}

bool PetState::load(const QString &filePath) {
    m_savePath = filePath;
    m_lastError.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        // Lần đầu chạy — tạo file với giá trị mặc định
        qDebug() << "pet_state.json không tìm thấy, dùng giá trị mặc định.";
        save();
        return true;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        m_lastError = "pet_state.json bị hỏng.";
        return false;
    }

    const QJsonObject obj = doc.object();
    m_hunger          = obj["hunger"].toInt(0);
    m_petSize         = obj["pet_size"].toInt(SizeCfg::kDefault);
    m_lifetimeEatenMb = static_cast<qint64>(obj["lifetime_eaten_mb"].toDouble(0));
    m_costume         = obj["costume"].toString();
    m_aiProvider      = obj["ai_provider"].toString();

    // Tính hunger tăng khi offline
    const QString lastSaveIso = obj["last_save_iso"].toString();
    if (!lastSaveIso.isEmpty()) {
        const QDateTime lastSave = QDateTime::fromString(lastSaveIso, Qt::ISODate);
        if (lastSave.isValid()) {
            const qint64 secsOffline = lastSave.secsTo(QDateTime::currentDateTime());
            if (secsOffline > 0) {
                const int tickSec  = HungerCfg::kTickMs / 1000;
                const int increase = static_cast<int>(secsOffline / tickSec);
                m_hunger = m_hunger + increase;
            }
        }
    }

    setHunger(m_hunger);   // clamp [0, kHungerMax]
    setPetSize(m_petSize); // clamp [kMin, kMax]
    return true;
}

bool PetState::save() const {
    if (m_savePath.isEmpty()) return false;

    // Đảm bảo thư mục cha tồn tại
    QDir().mkpath(QFileInfo(m_savePath).absolutePath());

    QJsonObject obj;
    obj["hunger"]           = m_hunger;
    obj["pet_size"]         = m_petSize;
    obj["last_save_iso"]    = QDateTime::currentDateTime().toString(Qt::ISODate);
    obj["lifetime_eaten_mb"] = static_cast<double>(m_lifetimeEatenMb);
    obj["costume"]          = m_costume;
    obj["ai_provider"]      = m_aiProvider;

    QFile f(m_savePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Không thể lưu pet_state.json:" << m_savePath;
        return false;
    }
    f.write(QJsonDocument(obj).toJson());
    return true;
}

void PetState::setHunger(int h) {
    m_hunger = std::clamp(h, 0, HungerCfg::kHungerMax);
}

void PetState::setPetSize(int s) {
    m_petSize = std::clamp(s, SizeCfg::kMin, SizeCfg::kMax);
}
