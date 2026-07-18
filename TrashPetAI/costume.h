#ifndef COSTUME_H
#define COSTUME_H

#include <QString>
#include <QVector>
#include <QPair>

// id rỗng ("") = trang phục mặc định (slime). id khác = tên file trong assets/costumes/<id>.png
namespace CostumeCatalog {

struct Entry {
    QString id;
    QString displayName;
};

inline const QVector<Entry>& all() {
    static const QVector<Entry> list = {
        { "",              QStringLiteral("🟢 Slime (mặc định)") },
        { "human_hero",    QStringLiteral("🧑 Anh hùng nhỏ") },
        { "human_rpg",     QStringLiteral("🧑 Người RPG") },
        { "human_player",  QStringLiteral("🧑 Chiến binh") },
        { "goblin_knight", QStringLiteral("👺 Goblin hiệp sĩ") },
        { "goblin_render", QStringLiteral("👺 Goblin") },
        { "knight",        QStringLiteral("⚔️ Kỵ sĩ") },
        { "orc_hero",      QStringLiteral("🟩 Orc anh hùng") },
        { "orc_warrior",   QStringLiteral("🟩 Orc chiến binh") },
        { "orc_female",    QStringLiteral("🟩 Orc nữ") },
        { "skeleton",      QStringLiteral("💀 Bộ xương") },
    };
    return list;
}

inline QString displayNameFor(const QString &id) {
    for (const auto &e : all()) {
        if (e.id == id) return e.displayName;
    }
    return QStringLiteral("🟢 Slime (mặc định)");
}

} // namespace CostumeCatalog

#endif // COSTUME_H
