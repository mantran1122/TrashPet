#ifndef HUNGERCONFIG_H
#define HUNGERCONFIG_H

#include <QtGlobal>

namespace HungerCfg {
    constexpr int     kTickMs               = 60 * 1000;  // 1 phút/tick (100 phút full đói từ 0)
    constexpr int     kHungerPerTick        = 1;
    constexpr int     kHungerMax            = 100;
    constexpr int     kThresholdMild        = 40;
    constexpr int     kThresholdHungry      = 70;
    constexpr int     kReminderIntervalTicks = 10;         // nhắc lại mỗi 10 tick (~10 phút) khi vẫn đói

    constexpr quint64 kMaxFileBytes    = 1ULL << 30; // 1 GB
    constexpr int     kMBPerHunger     = 1;           // 1 MB = -1 hunger
    constexpr int     kOverflowToSize  = 10;          // 10 MB overflow = +1 px
    constexpr int     kEatDelayMs      = 300;         // delay per eaten file
}

namespace SizeCfg {
    constexpr int kMin                    = 80;
    constexpr int kDefault                = 160;
    constexpr int kMax                    = 320;
    constexpr int kShrinkStartHunger      = 70;
    constexpr int kHungerTicksPerSizeLoss = 6;        // 1 px / 6 phút khi rất đói (kTickMs=1 phút)
}

#endif // HUNGERCONFIG_H
