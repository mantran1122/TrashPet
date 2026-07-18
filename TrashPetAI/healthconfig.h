#ifndef HEALTHCONFIG_H
#define HEALTHCONFIG_H

#include <QtGlobal>

namespace HealthCfg {
    constexpr int     kCheckMs            = 30 * 1000;  // 30s/tick (test mode)
    constexpr double  kDiskFreePercentMin = 10.0;        // < 10% free → warning
    constexpr quint64 kDiskFreeBytesMin   = 5ULL << 30;  // < 5 GB free → warning
    constexpr double  kRamUsedPercentMax  = 60.0;        // > 60% used → warning (test mode, máy 32GB)
}

#endif // HEALTHCONFIG_H
