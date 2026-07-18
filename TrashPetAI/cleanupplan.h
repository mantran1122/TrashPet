#ifndef CLEANUPPLAN_H
#define CLEANUPPLAN_H

#include <QVector>
#include "recyclebinmanager.h"

// Chính sách chọn file để ăn.
// ClosestFit: greedy "best-fit decreasing" — mỗi bước chọn file lớn nhất còn
// vừa với hunger còn lại; nếu không file nào vừa, chọn file nhỏ nhất vượt
// hunger còn lại (overshoot tối thiểu). Giảm overshoot so với 2 baseline kia.
enum class EatPolicy { OldestFirst, LargestFirst, ClosestFit };

// Kế hoạch "ăn rác" — nguồn logic DUY NHẤT cho việc chọn file sẽ xóa.
// EatDialog (preview) và ChatWidget (thực thi) phải dùng chung một CleanupPlan
// để tránh hai nơi tính ra hai danh sách khác nhau.
struct CleanupPlan {
    QVector<RecycleBinManager::Item> selectedItems;  // sẽ bị xóa, theo đúng thứ tự ăn
    QVector<RecycleBinManager::Item> skippedItems;   // > 1 GB, không đụng tới

    quint64 plannedBytes           = 0;  // tổng bytes THỰC sẽ mất (không cap theo hunger)
    int     plannedHungerReduction = 0;  // phần hunger thực sự giảm được (có cap)
};

// Chọn file theo `policy` cho đến khi hunger về 0.
// File > 1 GB luôn bị loại ra skippedItems trước.
CleanupPlan buildCleanupPlan(const QVector<RecycleBinManager::Item> &items,
                              int hunger,
                              EatPolicy policy);

#endif // CLEANUPPLAN_H
