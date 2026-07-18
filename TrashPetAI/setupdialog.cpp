#include "setupdialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

SetupDialog::SetupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("TrashPet AI — Cài đặt AI");
    setFixedWidth(420);

    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(
        "🐌 Chào bạn! Chọn AI muốn dùng và dán API key vào bên dưới nhé.\n"
        "Key được lưu trên máy bạn, không gửi đi đâu khác ngoài provider bạn chọn.",
        this);
    title->setWordWrap(true);
    layout->addWidget(title);

    layout->addSpacing(6);
    layout->addWidget(new QLabel("AI provider:", this));
    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem("Gemini (Google)", "gemini");
    m_providerCombo->addItem("ChatGPT (OpenAI)", "openai");
    m_providerCombo->addItem("Claude (Anthropic)", "claude");
    layout->addWidget(m_providerCombo);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet("color: #888; font-size: 11px;");
    m_hintLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_hintLabel->setOpenExternalLinks(true);
    layout->addWidget(m_hintLabel);

    layout->addSpacing(6);
    layout->addWidget(new QLabel("API key:", this));
    auto *keyRow = new QHBoxLayout();
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setPlaceholderText("Dán API key vào đây...");
    m_showKeyCheck = new QCheckBox("Hiện", this);
    keyRow->addWidget(m_keyEdit, 1);
    keyRow->addWidget(m_showKeyCheck);
    layout->addLayout(keyRow);

    auto *btnRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton("Để sau", this);
    auto *okBtn = new QPushButton("Lưu & bắt đầu", this);
    okBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SetupDialog::onProviderChanged);
    connect(m_showKeyCheck, &QCheckBox::toggled, this, &SetupDialog::onToggleShowKey);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &SetupDialog::onAcceptClicked);

    updateHintLabel();
}

void SetupDialog::setInitialProvider(const QString &id)
{
    const int idx = m_providerCombo->findData(id);
    if (idx >= 0) m_providerCombo->setCurrentIndex(idx);
    updateHintLabel();
}

void SetupDialog::setInitialApiKey(const QString &key)
{
    m_keyEdit->setText(key);
}

QString SetupDialog::selectedProvider() const
{
    return m_providerCombo->currentData().toString();
}

QString SetupDialog::apiKey() const
{
    return m_keyEdit->text().trimmed();
}

void SetupDialog::onProviderChanged(int)
{
    updateHintLabel();
}

void SetupDialog::onToggleShowKey(bool checked)
{
    m_keyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void SetupDialog::updateHintLabel()
{
    const QString provider = selectedProvider();
    if (provider == "openai") {
        m_hintLabel->setText("Lấy key tại: platform.openai.com/api-keys");
    } else if (provider == "claude") {
        m_hintLabel->setText("Lấy key tại: console.anthropic.com/settings/keys");
    } else {
        m_hintLabel->setText("Lấy key miễn phí tại: aistudio.google.com/apikey");
    }
}

void SetupDialog::onAcceptClicked()
{
    if (apiKey().isEmpty()) {
        QMessageBox::warning(this, "Thiếu API key", "Bạn cần nhập API key để TrashPet có thể trò chuyện nhé.");
        return;
    }
    accept();
}
