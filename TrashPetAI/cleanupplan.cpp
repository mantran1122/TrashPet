#include "cleanupplan.h"
#include "hungerconfig.h"

#include <algorithm>
#include <limits>

namespace {
int hungerMbForBytes(quint64 bytes) {
    constexpr quint64 kOneMb = 1024ULL * 1024ULL;
    return static_cast<int>((bytes + kOneMb - 1) / kOneMb);
}

// Best-fit-decreasing greedy: ở mỗi bước, ưu tiên file lớn nhất còn vừa với
// hunger còn lại (không gây overshoot); nếu không còn file nào vừa, chọn file
// nhỏ nhất trong số vượt quá (overshoot tối thiểu có thể) rồi dừng.
void selectClosestFit(QVector<RecycleBinManager::Item> pool, int hunger, CleanupPlan &plan) {
    int remaining = hunger;
    while (remaining > 0 && !pool.isEmpty()) {
        int fitIdx = -1, fitMb = -1;
        int overIdx = -1, overMb = std::numeric_limits<int>::max();

        for (int i = 0; i < pool.size(); ++i) {
            const int mb = hungerMbForBytes(pool[i].sizeBytes);
            if (mb <= remaining) {
                if (mb > fitMb) { fitMb = mb; fitIdx = i; }
            } else if (mb < overMb) {
                overMb = mb; overIdx = i;
            }
        }

        const int chosen = (fitIdx != -1) ? fitIdx : overIdx;
        const RecycleBinManager::Item item = pool[chosen];
        pool.remove(chosen);

        plan.selectedItems.append(item);
        plan.plannedBytes += item.sizeBytes;
        const int mb = hungerMbForBytes(item.sizeBytes);
        const int consume = std::min(mb, remaining);
        plan.plannedHungerReduction += consume;
        remaining -= consume;
    }
}

void selectSorted(QVector<RecycleBinManager::Item> eligible, int hunger,
                   bool largestFirst, CleanupPlan &plan) {
    if (largestFirst) {
        std::sort(eligible.begin(), eligible.end(),
                  [](const RecycleBinManager::Item &a, const RecycleBinManager::Item &b) {
                      return a.sizeBytes > b.sizeBytes;
                  });
    } else {
        std::sort(eligible.begin(), eligible.end(),
                  [](const RecycleBinManager::Item &a, const RecycleBinManager::Item &b) {
                      return a.deletedAt < b.deletedAt;
                  });
    }

    int hungerLeft = hunger;
    for (const auto &it : eligible) {
        if (hungerLeft <= 0) break;
        plan.selectedItems.append(it);
        plan.plannedBytes += it.sizeBytes;
        const int mb = hungerMbForBytes(it.sizeBytes);
        const int consume = std::min(mb, hungerLeft);
        plan.plannedHungerReduction += consume;
        hungerLeft -= consume;
    }
}
}

CleanupPlan buildCleanupPlan(const QVector<RecycleBinManager::Item> &items,
                              int hunger,
                              EatPolicy policy)
{
    CleanupPlan plan;

    QVector<RecycleBinManager::Item> eligible;
    for (const auto &it : items) {
        if (it.sizeBytes > HungerCfg::kMaxFileBytes)
            plan.skippedItems.append(it);
        else
            eligible.append(it);
    }

    switch (policy) {
    case EatPolicy::ClosestFit:
        selectClosestFit(eligible, hunger, plan);
        break;
    case EatPolicy::LargestFirst:
        selectSorted(eligible, hunger, true, plan);
        break;
    case EatPolicy::OldestFirst:
        selectSorted(eligible, hunger, false, plan);
        break;
    }

    return plan;
}
