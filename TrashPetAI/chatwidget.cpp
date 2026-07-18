#include "chatwidget.h"
#include "characterwidget.h"
#include "geminiclient.h"
#include "openaiclient.h"
#include "claudeclient.h"
#include "configmanager.h"
#include "petstate.h"
#include "hungermanager.h"
#include "hungerconfig.h"
#include "recyclebinmanager.h"
#include "eatdialog.h"
#include "cleanupplan.h"
#include "systemmonitor.h"
#include "healthconfig.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <QDebug>
#include <algorithm>

namespace {
QString formatSize(quint64 bytes) {
    if (bytes >= 1024ULL * 1024 * 1024)
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}

int hungerMbForBytes(quint64 bytes) {
    constexpr quint64 kOneMb = 1024ULL * 1024ULL;
    return static_cast<int>((bytes + kOneMb - 1) / kOneMb);
}

// Nhận diện câu hỏi về tình trạng máy (ổ đĩa/RAM) để trả lời trực tiếp bằng
// dữ liệu thật từ SystemMonitor, không cần gửi lên AI (nhanh, chính xác, đỡ tốn quota).
bool isHealthQuery(const QString &text) {
    static const QStringList keywords = {
        "tình trạng máy", "tinh trang may",
        "máy mình", "may minh", "máy tính", "may tinh", "máy tao", "may tao",
        "kiểm tra máy", "kiem tra may", "báo cáo hệ thống", "bao cao he thong",
        "máy", "hệ thống", "he thong",
        "ổ đĩa", "o dia", "ổ cứng", "o cung",
        "dung lượng", "dung luong",
        "bộ nhớ", "bo nho", "ram",
        "còn trống", "con trong", "đầy chưa", "day chua", "sắp đầy", "sap day",
    };
    const QString lower = text.toLower();
    for (const QString &kw : keywords) {
        if (lower.contains(kw))
            return true;
    }
    return false;
}

QString buildHealthReport(SystemMonitor *monitor) {
    if (!monitor)
        return "Mình chưa theo dõi được tình trạng máy lúc này. 😅";

    QStringList lines;
    const QList<SystemMonitor::DiskInfo> drives = monitor->listFixedDrives();
    for (const auto &d : drives) {
        const bool low = d.freePercent < HealthCfg::kDiskFreePercentMin
                       || d.freeBytes  < HealthCfg::kDiskFreeBytesMin;
        lines << QString("%1 %2  còn %3 trống / %4 (%5%)")
                     .arg(low ? "⚠️" : "✅", d.driveLetter,
                          formatSize(d.freeBytes), formatSize(d.totalBytes))
                     .arg(d.freePercent, 0, 'f', 0);
    }

    const SystemMonitor::RamInfo ram = monitor->currentRamInfo();
    const bool ramHigh = ram.usedPercent > HealthCfg::kRamUsedPercentMax;
    lines << QString("%1 RAM: đang dùng %2% (%3 / %4)")
                 .arg(ramHigh ? "⚠️" : "✅")
                 .arg(ram.usedPercent, 0, 'f', 0)
                 .arg(formatSize(ram.totalBytes - ram.availBytes), formatSize(ram.totalBytes));

    return "💻 Tình trạng máy hiện tại:\n" + lines.join("\n");
}
}

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(380, 480);

    setStyleSheet(R"(
        QWidget { background-color: #1e1e2e; color: #e0e0e0; border-radius: 12px; }
        QTextEdit {
            background-color: #2a2a3e; border: 1px solid #3a3a4e;
            border-radius: 8px; padding: 8px; font-size: 13px;
        }
        QLineEdit {
            background-color: #2a2a3e; border: 1px solid #3a3a4e;
            border-radius: 8px; padding: 6px 10px; font-size: 13px;
        }
        QPushButton {
            background-color: #5b6fed; color: white; border: none;
            border-radius: 8px; padding: 6px 14px; font-weight: bold;
        }
        QPushButton:hover  { background-color: #6f81f0; }
        QPushButton:pressed{ background-color: #4a5fdb; }
        QPushButton:disabled{ background-color: #555; color: #aaa; }
    )");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Title row: tên + hunger label
    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel("💬 TrashPet AI", this);
    title->setStyleSheet("font-size: 15px; font-weight: bold; padding: 4px;");
    m_hungerLabel = new QLabel("🍃 Đói: 0/100", this);
    m_hungerLabel->setStyleSheet("font-size: 11px; color: #69db7c;");
    m_hungerLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(m_hungerLabel);
    mainLayout->addLayout(titleRow);

    // Hunger progress bar
    m_hungerBar = new QProgressBar(this);
    m_hungerBar->setRange(0, 100);
    m_hungerBar->setValue(0);
    m_hungerBar->setTextVisible(false);
    m_hungerBar->setFixedHeight(6);
    m_hungerBar->setStyleSheet(
        "QProgressBar { border: none; border-radius: 3px; background: #2a2a3e; }"
        "QProgressBar::chunk { background: #69db7c; border-radius: 3px; }");
    mainLayout->addWidget(m_hungerBar);

    m_chatArea = new QTextEdit(this);
    m_chatArea->setReadOnly(true);
    mainLayout->addWidget(m_chatArea, 1);

    auto *inputRow = new QHBoxLayout();
    m_inputBox = new QLineEdit(this);
    m_inputBox->setPlaceholderText("Nhập câu hỏi...");
    m_sendButton = new QPushButton("Gửi", this);
    inputRow->addWidget(m_inputBox, 1);
    inputRow->addWidget(m_sendButton);
    mainLayout->addLayout(inputRow);

    m_eatButton = new QPushButton("🗑️ Ăn rác", this);
    m_eatButton->setStyleSheet("background-color: #4a8a4a;");
    mainLayout->addWidget(m_eatButton);

    m_geminiClient = new GeminiClient(this);
    m_openaiClient = new OpenAiClient(this);
    m_claudeClient = new ClaudeClient(this);
    for (AiClient *client : {m_geminiClient, m_openaiClient, m_claudeClient}) {
        connect(client, &AiClient::responseReceived, this, &ChatWidget::onAiResponse);
        connect(client, &AiClient::errorOccurred,    this, &ChatWidget::onAiError);
    }

    m_aiProvider = ConfigManager::instance().aiProvider();
    if (m_aiProvider.isEmpty()) m_aiProvider = "gemini";
    const QString savedProvider = PetState::instance().aiProvider();
    if (!savedProvider.isEmpty()) m_aiProvider = savedProvider;

    connect(m_sendButton, &QPushButton::clicked,    this, &ChatWidget::onSendClicked);
    connect(m_inputBox,   &QLineEdit::returnPressed, this, &ChatWidget::onSendClicked);
    connect(m_eatButton,  &QPushButton::clicked,    this, &ChatWidget::triggerEatTrash);

    appendMessage("Bot", "Xin chào! Mình là TrashPet, mình có thể giúp gì cho bạn?");
}

ChatWidget::~ChatWidget() {}

void ChatWidget::setCharacterWidget(CharacterWidget *cw)
{
    m_character = cw;
    if (cw && cw->hungerManager()) {
        connect(cw->hungerManager(), &HungerManager::hungerChanged,
                this, &ChatWidget::onHungerChanged);
        onHungerChanged(cw->hungerManager()->currentHunger());
    }
}

void ChatWidget::onHungerChanged(int hunger)
{
    m_hungerBar->setValue(hunger);
    m_hungerLabel->setText(QString("🍃 Đói: %1/100").arg(hunger));

    // Màu thanh: xanh → cam → đỏ theo mức đói
    QString color = hunger >= 70 ? "#ff6b6b"
                  : hunger >= 40 ? "#ffa94d"
                                 : "#69db7c";
    m_hungerBar->setStyleSheet(
        QString("QProgressBar { border: none; border-radius: 3px; background: #2a2a3e; }"
                "QProgressBar::chunk { background: %1; border-radius: 3px; }").arg(color));
    m_hungerLabel->setStyleSheet(
        QString("font-size: 11px; color: %1;").arg(color));
}

void ChatWidget::showNearCharacter(const QPoint &characterPos)
{
    constexpr int kGap     = 8;
    constexpr int kMargin  = 10;
    const int petSize = m_character ? m_character->petSize() : SizeCfg::kDefault;

    QScreen *screen = QGuiApplication::screenAt(characterPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect avail = screen->availableGeometry();

    const int chatW = width();
    const int chatH = height();

    int xLeft  = characterPos.x() - chatW - kGap;
    int xRight = characterPos.x() + petSize + kGap;

    int x;
    if (xLeft >= avail.left() + kMargin)
        x = xLeft;
    else if (xRight + chatW <= avail.right() - kMargin)
        x = xRight;
    else
        x = avail.left() + (avail.width() - chatW) / 2;

    int y = characterPos.y() + (petSize / 2) - (chatH / 2);

    if (x < avail.left() + kMargin)            x = avail.left() + kMargin;
    if (x + chatW > avail.right() - kMargin)   x = avail.right() - kMargin - chatW;
    if (y < avail.top() + kMargin)             y = avail.top() + kMargin;
    if (y + chatH > avail.bottom() - kMargin)  y = avail.bottom() - kMargin - chatH;

    move(x, y);
    show();
    raise();
    activateWindow();
    m_inputBox->setFocus();
}

void ChatWidget::appendMessage(const QString &sender, const QString &text)
{
    const QString color = (sender == "Bot") ? "#7ee787" : "#79c0ff";
    const QString html  = QString("<p><b style='color:%1'>%2:</b> %3</p>")
                              .arg(color, sender, text.toHtmlEscaped());
    m_chatArea->append(html);
}

void ChatWidget::onSendClicked()
{
    const QString text = m_inputBox->text().trimmed();
    if (text.isEmpty()) return;

    appendMessage("Bạn", text);
    m_inputBox->clear();

    m_inputBox->setEnabled(false);
    m_sendButton->setEnabled(false);
    appendMessage("Bot", "💭 Đang suy nghĩ...");

    if (m_character) m_character->setState(CharacterState::Thinking);

    AiClient *active = m_geminiClient;
    if (m_aiProvider == "openai") active = m_openaiClient;
    else if (m_aiProvider == "claude") active = m_claudeClient;

    // Câu hỏi liên quan tới tình trạng máy: gắn kèm dữ liệu thật từ SystemMonitor
    // vào prompt để AI trả lời có căn cứ, không bịa số liệu (AI không tự đọc được máy).
    if (isHealthQuery(text)) {
        const QString report = buildHealthReport(m_character ? m_character->systemMonitor() : nullptr);
        const QString augmented = QString(
            "[Dữ liệu máy thật, dùng số liệu này để trả lời, không tự bịa số khác]\n%1\n\n"
            "[Câu hỏi của người dùng]\n%2").arg(report, text);
        active->sendMessage(augmented);
        return;
    }

    active->sendMessage(text);
}

void ChatWidget::onAiResponse(const QString &text)
{
    appendMessage("Bot", text);
    m_inputBox->setEnabled(true);
    m_sendButton->setEnabled(true);
    m_inputBox->setFocus();
    if (m_character) m_character->setState(CharacterState::Talking);
}

void ChatWidget::onAiError(const QString &errorMessage)
{
    appendMessage("Bot", "⚠️ " + errorMessage);
    m_inputBox->setEnabled(true);
    m_sendButton->setEnabled(true);
    if (m_character) m_character->setState(CharacterState::Error);
}

void ChatWidget::setAiProvider(const QString &id)
{
    if (m_aiProvider == id) return;
    m_aiProvider = id;
    PetState::instance().setAiProvider(id);
    PetState::instance().save();

    const QString label = (id == "openai") ? "ChatGPT"
                         : (id == "claude") ? "Claude"
                                            : "Gemini";
    appendMessage("Bot", QString("Đã chuyển sang dùng %1 rồi nha! 🤖").arg(label));
}

// ─────────────────────────────────────────────
//  Eat trash logic
// ─────────────────────────────────────────────

void ChatWidget::triggerEatTrash()
{
    HungerManager *hm = m_character ? m_character->hungerManager() : nullptr;

    // Kiểm tra no chưa
    if (hm && hm->currentHunger() == 0) {
        appendMessage("Bot", "Mình no rồi, ăn không được nữa đâu! 😌");
        return;
    }

    // Liệt kê Recycle Bin
    RecycleBinManager rbm;
    const QVector<RecycleBinManager::Item> allItems = rbm.listItems();

    if (!rbm.lastError().isEmpty()) {
        appendMessage("Bot", "⚠️ " + rbm.lastError());
        if (m_character) m_character->setState(CharacterState::Error);
        return;
    }

    if (allItems.isEmpty()) {
        appendMessage("Bot", "Không có gì để ăn hết á. Thùng rác đang trống. 🪣");
        if (m_character) m_character->setState(CharacterState::Happy);
        return;
    }

    // Hiển thị dialog xác nhận
    const int hunger = hm ? hm->currentHunger() : HungerCfg::kHungerMax;
    EatDialog dlg(allItems, hunger, this);
    if (dlg.exec() != QDialog::Accepted) return;

    // Dùng ĐÚNG plan mà EatDialog đã hiển thị cho người dùng — không tính lại,
    // tránh lệch giữa preview và thực thi (ví dụ preview 2 file nhưng confirm
    // lại tính trên toàn bộ danh sách hợp lệ).
    const CleanupPlan &plan = dlg.plan();

    m_eatQueue.clear();
    m_skippedNames.clear();
    m_failedNames.clear();
    m_eatenCount = 0;
    m_deletedBytes = 0;
    m_hungerConsumedMb = 0;

    m_eatQueue = plan.selectedItems;
    for (const auto &item : plan.skippedItems) {
        m_skippedNames.append(
            QString("%1 (%2)").arg(item.displayName, formatSize(item.sizeBytes)));
    }

    if (m_eatQueue.isEmpty()) {
        appendMessage("Bot", "Tất cả file đều > 1 GB, mình không ăn được. 😅");
        if (!m_skippedNames.isEmpty())
            appendMessage("Bot", "Đã skip: " + m_skippedNames.join(", "));
        return;
    }

    // Xác nhận xóa vĩnh viễn (nếu bật trong config) — file bị xóa hẳn,
    // không qua Recycle Bin lần nữa nên cần xác nhận rõ ràng trước khi tiến hành.
    if (ConfigManager::instance().confirmEmptyTrash()) {
        const auto reply = QMessageBox::question(
            this, "Xác nhận xóa vĩnh viễn",
            QString("TrashPet sẽ xóa vĩnh viễn %1 file (~%2), không thể khôi phục.\nTiếp tục?")
                .arg(plan.selectedItems.size()).arg(formatSize(plan.plannedBytes)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            appendMessage("Bot", "Đã hủy, không ăn gì cả. 🙅");
            return;
        }
    }

    // Bắt đầu ăn
    appendMessage("Bot", "Đợi mình ăn xíu nha... 😋");
    if (m_character) m_character->setState(CharacterState::Eating);
    m_eatButton->setEnabled(false);

    if (!m_eatTimer) {
        m_eatTimer = new QTimer(this);
        m_eatTimer->setInterval(HungerCfg::kEatDelayMs);
        connect(m_eatTimer, &QTimer::timeout, this, &ChatWidget::onEatTick);
    }
    m_eatTimer->start();
}

void ChatWidget::onEatTick()
{
    // Dừng nếu no hoặc hết file
    HungerManager *hm = m_character ? m_character->hungerManager() : nullptr;
    if ((hm && hm->currentHunger() == 0) || m_eatQueue.isEmpty()) {
        finishEating();
        return;
    }

    const RecycleBinManager::Item item = m_eatQueue.takeFirst();
    RecycleBinManager rbm;
    if (rbm.deleteItem(item)) {
        const int mb = hungerMbForBytes(item.sizeBytes);
        m_deletedBytes += item.sizeBytes;
        if (hm) m_hungerConsumedMb += hm->feedMb(mb);
        if (m_character) m_character->playEatEffect(QString("+%1 MB").arg(mb));
        m_eatenCount++;
    } else {
        qWarning() << "TrashPet: xóa thất bại" << item.displayName << rbm.lastError();
        m_failedNames.append(QString("%1 (%2)").arg(item.displayName, rbm.lastError()));
    }
}

void ChatWidget::finishEating()
{
    m_eatTimer->stop();
    m_eatButton->setEnabled(true);

    if (m_eatenCount == 0) {
        appendMessage("Bot", "Ôi, mình không ăn được gì cả... 😢");
        if (m_character) m_character->setState(CharacterState::Error);
    } else {
        appendMessage("Bot", QString("Đã xóa thành công %1 file, giải phóng %2.\nHunger giảm %3 điểm.")
                                  .arg(m_eatenCount)
                                  .arg(formatSize(m_deletedBytes))
                                  .arg(m_hungerConsumedMb));
        if (m_character) m_character->setState(CharacterState::Happy);
    }

    if (!m_skippedNames.isEmpty()) {
        appendMessage("Bot", "Đã skip (> 1 GB): " + m_skippedNames.join(", "));
    }
    if (!m_failedNames.isEmpty()) {
        appendMessage("Bot", QString("⚠️ %1 file xóa thất bại: ").arg(m_failedNames.size())
                                  + m_failedNames.join(", "));
    }

    m_eatQueue.clear();
    m_skippedNames.clear();
    m_failedNames.clear();
    m_eatenCount = 0;
    m_deletedBytes = 0;
    m_hungerConsumedMb = 0;
}
