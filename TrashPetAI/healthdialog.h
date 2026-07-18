#ifndef HEALTHDIALOG_H
#define HEALTHDIALOG_H

#include <QDialog>

class QVBoxLayout;
class SystemMonitor;

class HealthDialog : public QDialog {
    Q_OBJECT
public:
    explicit HealthDialog(SystemMonitor *monitor, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    SystemMonitor *m_monitor = nullptr;
    QVBoxLayout    *m_contentLayout = nullptr;
};

#endif // HEALTHDIALOG_H
