#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

class QTimer;

class SystemMonitor : public QObject {
    Q_OBJECT
public:
    struct DiskInfo {
        QString driveLetter;   // vd "C:"
        quint64 freeBytes  = 0;
        quint64 totalBytes = 0;
        double  freePercent = 0.0;
    };

    struct RamInfo {
        double  usedPercent = 0.0;
        quint64 totalBytes  = 0;
        quint64 availBytes  = 0;
    };

    explicit SystemMonitor(QObject *parent = nullptr);

    void start();
    void stop();

    // Liệt kê tất cả ổ DRIVE_FIXED gắn trong máy
    QList<DiskInfo> listFixedDrives() const;
    RamInfo currentRamInfo() const;

signals:
    // Phát khi 1 ổ vượt ngưỡng lần đầu (chưa notified); reset khi ổ trở lại bình thường
    void diskWarning(QString driveLetter, quint64 freeBytes, quint64 totalBytes);
    // Phát khi RAM vượt ngưỡng lần đầu; reset khi RAM trở lại bình thường
    void ramWarning(double usedPercent);

private slots:
    void onTick();

private:
    QTimer *m_timer = nullptr;
    QMap<QString, bool> m_driveNotified;
    bool m_ramNotified = false;
};

#endif // SYSTEMMONITOR_H
