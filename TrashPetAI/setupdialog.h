#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;

// Dialog hỏi người dùng chọn AI provider + nhập API key.
// Dùng cho lần chạy đầu tiên (chưa có key nào) và cho menu "⚙️ Cài đặt".
class SetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = nullptr);

    // "gemini" | "openai" | "claude"
    void setInitialProvider(const QString &id);
    void setInitialApiKey(const QString &key);

    QString selectedProvider() const;
    QString apiKey() const;

private slots:
    void onProviderChanged(int index);
    void onToggleShowKey(bool checked);
    void onAcceptClicked();

private:
    void updateHintLabel();

    QComboBox *m_providerCombo;
    QLineEdit *m_keyEdit;
    QCheckBox *m_showKeyCheck;
    QLabel    *m_hintLabel;
};

#endif // SETUPDIALOG_H
