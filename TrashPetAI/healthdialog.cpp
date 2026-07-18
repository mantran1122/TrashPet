#include "healthdialog.h"
#include "systemmonitor.h"
#include "healthconfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QProgressBar>
#include <QPushButton>

namespace {
QString formatSize(quint64 bytes) {
    if (bytes >= 1024ULL * 1024 * 1024)
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
}
}

HealthDialog::HealthDialog(SystemMonitor *monitor, QWidget *parent)
    : QDialog(parent)
    , m_monitor(monitor)
{
    setWindowTitle("💻 Tình trạng máy");
    setMinimumWidth(380);

    setStyleSheet(R"(
        QDialog  { background-color: #1e1e2e; color: #e0e0e0; }
        QLabel   { color: #e0e0e0; }
        QGroupBox{ color: #aaaaaa; border: 1px solid #3a3a4e; border-radius: 6px;
                   margin-top: 8px; padding: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QProgressBar { border: 1px solid #3a3a4e; border-radius: 6px; text-align: center;
                       color: #e0e0e0; background-color: #2a2a3e; }
        QProgressBar::chunk { background-color: #5b6fed; border-radius: 5px; }
        QPushButton {
            background-color: #5b6fed; color: white; border: none;
            border-radius: 8px; padding: 8px 18px; font-weight: bold;
        }
        QPushButton:hover { background-color: #6f81f0; }
    )");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    m_contentLayout = new QVBoxLayout();
    mainLayout->addLayout(m_contentLayout);

    auto *btnRefresh = new QPushButton("🔄 Làm mới", this);
    mainLayout->addWidget(btnRefresh);
    connect(btnRefresh, &QPushButton::clicked, this, &HealthDialog::refresh);

    refresh();
}

void HealthDialog::refresh()
{
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!m_monitor)
        return;

    const QList<SystemMonitor::DiskInfo> drives = m_monitor->listFixedDrives();
    auto *grpDisks = new QGroupBox("Ổ đĩa:", this);
    auto *disksLayout = new QVBoxLayout(grpDisks);
    for (const auto &d : drives) {
        auto *lbl = new QLabel(
            QString("%1  —  %2 trống / %3")
                .arg(d.driveLetter, formatSize(d.freeBytes), formatSize(d.totalBytes)),
            grpDisks);
        disksLayout->addWidget(lbl);

        auto *bar = new QProgressBar(grpDisks);
        bar->setRange(0, 100);
        bar->setValue(static_cast<int>(100.0 - d.freePercent));
        bar->setFormat(QString("%1% đã dùng").arg(static_cast<int>(100.0 - d.freePercent)));
        if (d.freePercent < HealthCfg::kDiskFreePercentMin || d.freeBytes < HealthCfg::kDiskFreeBytesMin) {
            bar->setStyleSheet("QProgressBar::chunk { background-color: #e05c5c; }");
        }
        disksLayout->addWidget(bar);
    }
    if (drives.isEmpty()) {
        disksLayout->addWidget(new QLabel("(Không tìm thấy ổ đĩa cứng nào)", grpDisks));
    }
    m_contentLayout->addWidget(grpDisks);

    const SystemMonitor::RamInfo ram = m_monitor->currentRamInfo();
    auto *grpRam = new QGroupBox("RAM:", this);
    auto *ramLayout = new QVBoxLayout(grpRam);
    ramLayout->addWidget(new QLabel(
        QString("Đang dùng %1 / %2").arg(formatSize(ram.totalBytes - ram.availBytes), formatSize(ram.totalBytes)),
        grpRam));
    auto *ramBar = new QProgressBar(grpRam);
    ramBar->setRange(0, 100);
    ramBar->setValue(static_cast<int>(ram.usedPercent));
    ramBar->setFormat(QString("%1% đã dùng").arg(static_cast<int>(ram.usedPercent)));
    if (ram.usedPercent > HealthCfg::kRamUsedPercentMax) {
        ramBar->setStyleSheet("QProgressBar::chunk { background-color: #e05c5c; }");
    }
    ramLayout->addWidget(ramBar);
    m_contentLayout->addWidget(grpRam);
}
