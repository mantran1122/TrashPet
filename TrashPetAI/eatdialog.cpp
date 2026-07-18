#include "eatdialog.h"
#include "hungerconfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include <QLayoutItem>

namespace {
// Format dung lượng bytes → chuỗi dễ đọc
QString formatSize(quint64 bytes) {
    if (bytes >= 1024ULL * 1024 * 1024)
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}
}

EatDialog::EatDialog(const QVector<RecycleBinManager::Item> &items,
                     int currentHunger,
                     QWidget *parent)
    : QDialog(parent)
    , m_allItems(items)
    , m_hunger(currentHunger)
{
    setWindowTitle("🗑️ Cho TrashPet ăn rác");
    setMinimumWidth(420);

    setStyleSheet(R"(
        QDialog  { background-color: #1e1e2e; color: #e0e0e0; }
        QLabel   { color: #e0e0e0; }
        QGroupBox{ color: #aaaaaa; border: 1px solid #3a3a4e; border-radius: 6px;
                   margin-top: 8px; padding: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QRadioButton { color: #e0e0e0; spacing: 6px; }
        QRadioButton::indicator {
            width: 14px; height: 14px; border-radius: 8px;
            border: 2px solid #7a7a90; background-color: #14141f;
        }
        QRadioButton::indicator:hover { border: 2px solid #8f9dfa; }
        QRadioButton::indicator:checked {
            border: 2px solid #8f9dfa; background-color: #5b6fed;
        }
        QListWidget { background-color: #14141f; border: 1px solid #3a3a4e; border-radius: 4px; }
        QPushButton {
            background-color: #5b6fed; color: white; border: none;
            border-radius: 8px; padding: 8px 18px; font-weight: bold;
        }
        QPushButton:hover { background-color: #6f81f0; }
        QPushButton#cancelBtn {
            background-color: #3a3a4e; color: #aaaaaa;
        }
        QPushButton#cancelBtn:hover { background-color: #4a4a5e; }
    )");

    quint64 totalBytes = 0;
    for (const auto &it : m_allItems) totalBytes += it.sizeBytes;

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // Header: trạng thái đói
    QString hungerEmoji = currentHunger >= HungerCfg::kThresholdHungry ? "🥺🥺🥺"
                        : currentHunger >= HungerCfg::kThresholdMild   ? "🥺"
                                                                        : "🐌";
    auto *lblHunger = new QLabel(
        QString("%1 TrashPet đang đói: <b>%2/100</b>")
            .arg(hungerEmoji).arg(currentHunger), this);
    lblHunger->setStyleSheet("font-size: 14px;");
    mainLayout->addWidget(lblHunger);

    // Thông tin thùng rác
    auto *lblInfo = new QLabel(
        QString("Thùng rác: <b>%1 file</b> (%2)")
            .arg(m_allItems.size()).arg(formatSize(totalBytes)), this);
    mainLayout->addWidget(lblInfo);

    // Thứ tự ăn
    auto *grpOrder = new QGroupBox("Thứ tự ăn:", this);
    auto *orderLayout = new QVBoxLayout(grpOrder);
    m_radioOldest     = new QRadioButton("🕐 Cũ nhất trước",  grpOrder);
    m_radioLargest    = new QRadioButton("📦 Lớn nhất trước", grpOrder);
    m_radioClosestFit = new QRadioButton("🎯 Vừa khít (giảm dư thừa)", grpOrder);
    m_radioOldest->setChecked(true);
    orderLayout->addWidget(m_radioOldest);
    orderLayout->addWidget(m_radioLargest);
    orderLayout->addWidget(m_radioClosestFit);
    mainLayout->addWidget(grpOrder);

    // Cảnh báo xóa vĩnh viễn: hiển thị đúng tổng dung lượng THỰC sẽ mất,
    // không phải phần hunger được cap — vì file luôn bị xóa nguyên vẹn.
    m_lblEstimate = new QLabel(this);
    m_lblEstimate->setWordWrap(true);
    m_lblEstimate->setStyleSheet("color: #ff8888; font-size: 12px; font-weight: bold;");
    mainLayout->addWidget(m_lblEstimate);
    m_lblHungerNote = new QLabel(this);
    m_lblHungerNote->setStyleSheet("color: #7ee787; font-size: 12px;");
    mainLayout->addWidget(m_lblHungerNote);

    // Preview: danh sách file THỰC SỰ sẽ bị xóa (không chỉ số lượng/tổng dung lượng)
    auto *grpPreview = new QGroupBox("Các file sẽ bị xóa:", this);
    auto *previewLayout = new QVBoxLayout(grpPreview);
    m_listPreview = new QListWidget(grpPreview);
    m_listPreview->setMaximumHeight(140);
    previewLayout->addWidget(m_listPreview);
    mainLayout->addWidget(grpPreview);

    // Skip list (nội dung điền trong refreshPlan(), vì phụ thuộc CleanupPlan)
    m_grpSkip = new QGroupBox(this);
    m_skipLayout = new QVBoxLayout(m_grpSkip);
    mainLayout->addWidget(m_grpSkip);
    m_grpSkip->setVisible(false);

    connect(m_radioOldest,     &QRadioButton::toggled, this, &EatDialog::refreshPlan);
    connect(m_radioLargest,    &QRadioButton::toggled, this, &EatDialog::refreshPlan);
    connect(m_radioClosestFit, &QRadioButton::toggled, this, &EatDialog::refreshPlan);

    refreshPlan();

    if (m_plan.selectedItems.isEmpty()) {
        auto *lblNoEat = new QLabel("(Không có file nào phù hợp để ăn)", this);
        lblNoEat->setStyleSheet("color: #ff8888;");
        mainLayout->addWidget(lblNoEat);
    }

    // Buttons
    auto *btnRow   = new QHBoxLayout();
    auto *btnEat   = new QPushButton("🍴 Cho ăn!", this);
    auto *btnCancel = new QPushButton("Hủy", this);
    btnCancel->setObjectName("cancelBtn");
    btnRow->addWidget(btnEat);
    btnRow->addWidget(btnCancel);
    mainLayout->addLayout(btnRow);

    btnEat->setEnabled(!m_plan.selectedItems.isEmpty());

    connect(btnEat,    &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void EatDialog::refreshPlan()
{
    const EatPolicy policy = m_radioClosestFit->isChecked() ? EatPolicy::ClosestFit
                            : m_radioLargest->isChecked()    ? EatPolicy::LargestFirst
                                                              : EatPolicy::OldestFirst;
    m_plan = buildCleanupPlan(m_allItems, m_hunger, policy);

    m_lblEstimate->setText(
        QString("⚠️ Sẽ xóa vĩnh viễn %1 file — tổng ~%2 (không thể khôi phục)")
            .arg(m_plan.selectedItems.size()).arg(formatSize(m_plan.plannedBytes)));
    m_lblHungerNote->setText(
        QString("Hunger giảm %1 MB (phần dư chuyển thành tăng kích thước pet)")
            .arg(m_plan.plannedHungerReduction));

    m_listPreview->clear();
    for (const auto &it : m_plan.selectedItems) {
        m_listPreview->addItem(QString("%1  —  %2").arg(it.displayName, formatSize(it.sizeBytes)));
    }

    QLayoutItem *child;
    while ((child = m_skipLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    if (m_plan.skippedItems.isEmpty()) {
        m_grpSkip->setVisible(false);
    } else {
        m_grpSkip->setTitle(QString("%1 file > 1 GB sẽ bị skip:").arg(m_plan.skippedItems.size()));
        for (const auto &it : m_plan.skippedItems) {
            auto *lbl = new QLabel(
                QString("  • %1  (%2)").arg(it.displayName, formatSize(it.sizeBytes)), m_grpSkip);
            lbl->setStyleSheet("color: #ff8888; font-size: 12px;");
            m_skipLayout->addWidget(lbl);
        }
        m_grpSkip->setVisible(true);
    }
}
