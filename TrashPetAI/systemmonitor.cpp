#include "systemmonitor.h"
#include "healthconfig.h"

#include <QTimer>
#include <windows.h>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(HealthCfg::kCheckMs);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitor::onTick);
}

void SystemMonitor::start() { m_timer->start(); }
void SystemMonitor::stop()  { m_timer->stop(); }

QList<SystemMonitor::DiskInfo> SystemMonitor::listFixedDrives() const
{
    QList<DiskInfo> result;

    const DWORD driveMask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(driveMask & (1u << i)))
            continue;

        const QChar letter = QChar('A' + i);
        const QString root = QString("%1:\\").arg(letter);
        const std::wstring rootW = root.toStdWString();

        if (GetDriveTypeW(rootW.c_str()) != DRIVE_FIXED)
            continue;

        ULARGE_INTEGER freeAvail, total, free;
        if (!GetDiskFreeSpaceExW(rootW.c_str(), &freeAvail, &total, &free))
            continue;
        if (total.QuadPart == 0)
            continue;

        DiskInfo info;
        info.driveLetter = QString("%1:").arg(letter);
        info.freeBytes   = freeAvail.QuadPart;
        info.totalBytes  = total.QuadPart;
        info.freePercent = 100.0 * static_cast<double>(info.freeBytes) / static_cast<double>(info.totalBytes);
        result.append(info);
    }

    return result;
}

SystemMonitor::RamInfo SystemMonitor::currentRamInfo() const
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);

    RamInfo info;
    info.totalBytes  = status.ullTotalPhys;
    info.availBytes  = status.ullAvailPhys;
    info.usedPercent = static_cast<double>(status.dwMemoryLoad);
    return info;
}

void SystemMonitor::onTick()
{
    const QList<DiskInfo> drives = listFixedDrives();
    for (const DiskInfo &d : drives) {
        const bool low = d.freePercent < HealthCfg::kDiskFreePercentMin
                       || d.freeBytes  < HealthCfg::kDiskFreeBytesMin;
        const bool alreadyNotified = m_driveNotified.value(d.driveLetter, false);

        if (low && !alreadyNotified) {
            m_driveNotified[d.driveLetter] = true;
            emit diskWarning(d.driveLetter, d.freeBytes, d.totalBytes);
        } else if (!low && alreadyNotified) {
            m_driveNotified[d.driveLetter] = false;
        }
    }

    const RamInfo ram = currentRamInfo();
    const bool ramHigh = ram.usedPercent > HealthCfg::kRamUsedPercentMax;
    if (ramHigh && !m_ramNotified) {
        m_ramNotified = true;
        emit ramWarning(ram.usedPercent);
    } else if (!ramHigh && m_ramNotified) {
        m_ramNotified = false;
    }
}
