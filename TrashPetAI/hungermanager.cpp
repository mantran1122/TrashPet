#include "hungermanager.h"
#include "hungerconfig.h"
#include "petstate.h"

#include <QTimer>
#include <algorithm>

HungerManager::HungerManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(HungerCfg::kTickMs);
    connect(m_timer, &QTimer::timeout, this, &HungerManager::onTick);
}

void HungerManager::start() {
    m_timer->start();
    // Báo ngay nếu pet đã đói sẵn từ lần trước (load từ PetState), không đợi tick đầu tiên.
    // Chạy qua singleShot(0) để CharacterWidget kịp connect() thresholdReached trước khi phát.
    QTimer::singleShot(0, this, [this]() { checkThresholds(currentHunger()); });
}
void HungerManager::stop()  { m_timer->stop(); }

int HungerManager::currentHunger() const {
    return PetState::instance().hunger();
}

int HungerManager::feedMb(int mb) {
    auto &ps = PetState::instance();
    const int before   = ps.hunger();
    const int consumed = std::min(mb, before);
    const int overflow = mb - consumed;

    ps.setHunger(before - consumed);

    if (overflow > 0) {
        const int sizeGain = overflow / HungerCfg::kOverflowToSize;
        if (sizeGain > 0) {
            const int beforeSize = ps.petSize();
            ps.setPetSize(ps.petSize() + sizeGain);
            if (ps.petSize() != beforeSize)
                emit petSizeChanged(ps.petSize());
        }
    }

    ps.addEatenMb(static_cast<qint64>(mb));

    // Reset notify flags nếu hunger về dưới ngưỡng
    const int newH = ps.hunger();
    if (newH < SizeCfg::kShrinkStartHunger)
        m_hungryTicks = 0;

    if (newH < HungerCfg::kThresholdMild)   m_mildNotified   = false;
    if (newH < HungerCfg::kThresholdHungry) { m_hungryNotified = false; m_hungryReminderTicks = 0; }

    emit hungerChanged(newH);
    return consumed;
}

void HungerManager::onTick() {
    auto &ps  = PetState::instance();
    const int newH = std::min(ps.hunger() + HungerCfg::kHungerPerTick,
                              HungerCfg::kHungerMax);
    ps.setHunger(newH);
    emit hungerChanged(newH);

    if (newH >= SizeCfg::kShrinkStartHunger && ps.petSize() > SizeCfg::kMin) {
        ++m_hungryTicks;
        if (m_hungryTicks >= SizeCfg::kHungerTicksPerSizeLoss) {
            m_hungryTicks = 0;
            const int beforeSize = ps.petSize();
            ps.setPetSize(ps.petSize() - 1);
            if (ps.petSize() != beforeSize)
                emit petSizeChanged(ps.petSize());
        }
    } else if (newH < SizeCfg::kShrinkStartHunger) {
        m_hungryTicks = 0;
    }

    checkThresholds(newH);
}

void HungerManager::checkThresholds(int newHunger) {
    if (newHunger >= HungerCfg::kThresholdHungry) {
        if (!m_hungryNotified) {
            m_hungryNotified      = true;
            m_mildNotified        = true;
            m_hungryReminderTicks = 0;
            emit thresholdReached(HungerCfg::kThresholdHungry);
        } else if (++m_hungryReminderTicks >= HungerCfg::kReminderIntervalTicks) {
            // Vẫn còn đói sau khi đã báo — nhắc lại định kỳ thay vì im re.
            m_hungryReminderTicks = 0;
            emit thresholdReached(HungerCfg::kThresholdHungry);
        }
    } else if (!m_mildNotified && newHunger >= HungerCfg::kThresholdMild) {
        m_mildNotified = true;
        emit thresholdReached(HungerCfg::kThresholdMild);
    }
}
