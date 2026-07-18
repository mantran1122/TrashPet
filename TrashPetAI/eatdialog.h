#ifndef EATDIALOG_H
#define EATDIALOG_H

#include <QDialog>
#include "recyclebinmanager.h"
#include "cleanupplan.h"

class QRadioButton;
class QLabel;
class QListWidget;
class QGroupBox;
class QVBoxLayout;

class EatDialog : public QDialog {
    Q_OBJECT
public:
    // items: toàn bộ item trong recycle bin (bao gồm cả > 1GB)
    // currentHunger: chỉ số đói hiện tại [0, 100]
    explicit EatDialog(const QVector<RecycleBinManager::Item> &items,
                       int currentHunger,
                       QWidget *parent = nullptr);

    // Kế hoạch ăn khớp với lựa chọn hiện tại của dialog (đã accept).
    // ChatWidget dùng trực tiếp plan này để thực thi — không tính lại.
    const CleanupPlan &plan() const { return m_plan; }

private slots:
    void refreshPlan();

private:
    QVector<RecycleBinManager::Item> m_allItems;
    int                               m_hunger = 0;
    CleanupPlan                       m_plan;

    QRadioButton *m_radioOldest     = nullptr;
    QRadioButton *m_radioLargest    = nullptr;
    QRadioButton *m_radioClosestFit = nullptr;
    QLabel       *m_lblEstimate   = nullptr;
    QLabel       *m_lblHungerNote = nullptr;
    QListWidget  *m_listPreview   = nullptr;
    QGroupBox    *m_grpSkip       = nullptr;
    QVBoxLayout  *m_skipLayout    = nullptr;
};

#endif // EATDIALOG_H
