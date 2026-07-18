#ifndef HUNGERMANAGER_H
#define HUNGERMANAGER_H

#include <QObject>

class QTimer;

class HungerManager : public QObject {
    Q_OBJECT
public:
    explicit HungerManager(QObject *parent = nullptr);

    void start();
    void stop();
    int  currentHunger() const;

    // Cho ăn: giảm hunger theo MB, overflow → tăng size pet
    // Trả về số MB thực sự tiêu thụ (hunger đã về 0 thì ít hơn mb)
    int  feedMb(int mb);

signals:
    void hungerChanged(int newHunger);
    void petSizeChanged(int newSize);
    // Phát khi vượt ngưỡng (40 hoặc 70); không phát lại cho đến khi hunger về dưới ngưỡng
    void thresholdReached(int threshold);

private slots:
    void onTick();

private:
    void checkThresholds(int newHunger);

    QTimer *m_timer               = nullptr;
    bool    m_mildNotified        = false;
    bool    m_hungryNotified      = false;
    int     m_hungryTicks         = 0;
    int     m_hungryReminderTicks = 0;  // đếm tick kể từ lần nhắc "đói" gần nhất
};

#endif // HUNGERMANAGER_H
