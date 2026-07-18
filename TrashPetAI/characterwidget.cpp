#include "characterwidget.h"
#include "chatwidget.h"
#include "hungermanager.h"
#include "hungerconfig.h"
#include "petstate.h"
#include "costume.h"
#include "configmanager.h"
#include "setupdialog.h"
#include "systemmonitor.h"
#include "healthdialog.h"

#include <QLabel>
#include <QMovie>
#include <QPixmap>
#include <QTransform>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QMenu>
#include <QActionGroup>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QRandomGenerator>
#include <QVector>
#include <QPair>
#include <algorithm>

namespace {
constexpr int kClickThreshold = 5;
constexpr int kTalkingMs      = 3000;   // Talking dài hơn (đang nói mà)
constexpr int kHappyMs        = 2000;   // Happy ngắn vừa phải
constexpr int kErrorMs        = 2500;   // Error đủ lâu để user thấy
constexpr int kSleepMs        = 5 * 60 * 1000;
constexpr int kEatEffectTicks = 12;
constexpr int kEatEffectMs    = 35;
constexpr int kRoamMs         = 80;
constexpr int kRoamMinTicks   = 35;
constexpr int kRoamMaxTicks   = 110;
constexpr int kRoamMargin     = 8;
}

CharacterWidget::CharacterWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    m_petSize = PetState::instance().petSize();
    m_costume.clear();  // tính năng trang phục đã tắt — luôn dùng slime mặc định
    resize(m_petSize, m_petSize);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_imageLabel->setStyleSheet("background: transparent;");

    m_eatEffectLabel = new QLabel(this);
    m_eatEffectLabel->setAlignment(Qt::AlignCenter);
    m_eatEffectLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_eatEffectLabel->setStyleSheet(
        "QLabel {"
        " background-color: rgba(30, 30, 46, 190);"
        " color: #ffe066;"
        " border: 1px solid rgba(255, 224, 102, 170);"
        " border-radius: 10px;"
        " padding: 3px 7px;"
        " font-weight: bold;"
        " font-size: 11px;"
        "}");
    m_eatEffectLabel->hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_imageLabel);

    loadAssets();

    m_revertTimer = new QTimer(this);
    m_revertTimer->setSingleShot(true);
    connect(m_revertTimer, &QTimer::timeout,
            this, &CharacterWidget::onAutoRevertToIdle);

    m_sleepTimer = new QTimer(this);
    m_sleepTimer->setSingleShot(true);
    connect(m_sleepTimer, &QTimer::timeout,
            this, &CharacterWidget::onSleepTimeout);
    m_sleepTimer->start(kSleepMs);

    m_eatEffectTimer = new QTimer(this);
    m_eatEffectTimer->setInterval(kEatEffectMs);
    connect(m_eatEffectTimer, &QTimer::timeout,
            this, &CharacterWidget::onEatEffectTick);

    m_roamTimer = new QTimer(this);
    m_roamTimer->setInterval(kRoamMs);
    connect(m_roamTimer, &QTimer::timeout,
            this, &CharacterWidget::onRoamTick);
    resetRoamDirection();
    m_roamTimer->start();

    // Hunger system — phải khởi tạo TRƯỚC ChatWidget
    m_hungerManager = new HungerManager(this);
    connect(m_hungerManager, &HungerManager::thresholdReached,
            this, &CharacterWidget::onHungerThreshold);
    connect(m_hungerManager, &HungerManager::petSizeChanged,
            this, &CharacterWidget::applyPetSize);
    connect(m_hungerManager, &HungerManager::hungerChanged, this, [this](int h) {
        setToolTip(QString("🍃 Độ đói: %1/100").arg(h));
    });
    setToolTip(QString("🍃 Độ đói: %1/100").arg(m_hungerManager->currentHunger()));
    m_hungerManager->start();

    // ChatWidget sau — lúc này hungerManager() đã có giá trị
    m_chatWidget = new ChatWidget(nullptr);
    m_chatWidget->setCharacterWidget(this);

    // Health monitor (disk/RAM)
    m_systemMonitor = new SystemMonitor(this);
    connect(m_systemMonitor, &SystemMonitor::diskWarning,
            this, &CharacterWidget::onDiskWarning);
    connect(m_systemMonitor, &SystemMonitor::ramWarning,
            this, &CharacterWidget::onRamWarning);
    m_systemMonitor->start();

    setState(CharacterState::Idle);
}

CharacterWidget::~CharacterWidget()
{
    delete m_happyPix;
    delete m_sleepingPix;
    delete m_errorPix;
    delete m_idlePixFallback;
    qDeleteAll(m_costumePixmaps);
}

QSize CharacterWidget::currentPetQSize() const
{
    return QSize(m_petSize, m_petSize);
}

QPixmap CharacterWidget::orientPixmap(const QPixmap &source) const
{
    if (source.isNull())
        return source;

    QPixmap scaled = source.scaled(currentPetQSize(),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    if (m_facingLeft)
        return scaled;
    return scaled.transformed(QTransform().scale(-1, 1));
}

void CharacterWidget::refreshCurrentMovieFrame()
{
    if (!m_currentMovie)
        return;

    const QPixmap frame = m_currentMovie->currentPixmap();
    if (!frame.isNull())
        m_imageLabel->setPixmap(orientPixmap(frame));
}

void CharacterWidget::setFacingFromVelocity(const QPoint &velocity)
{
    if (velocity.x() == 0)
        return;

    const bool shouldFaceLeft = velocity.x() < 0;
    if (m_facingLeft == shouldFaceLeft)
        return;

    m_facingLeft = shouldFaceLeft;
    if (m_currentMovie)
        refreshCurrentMovieFrame();
    else
        setState(m_state);
}

void CharacterWidget::loadAssets()
{
    const QStringList candidates = {
        QApplication::applicationDirPath() + "/assets/character",
        QApplication::applicationDirPath() + "/../assets/character",
        QApplication::applicationDirPath() + "/../../assets/character",
    };
    QString base;
    for (const auto &p : candidates) {
        if (QFileInfo::exists(p)) { base = p; break; }
    }
    if (base.isEmpty()) {
        qWarning() << "Không tìm thấy thư mục assets/character!";
        return;
    }

    auto makeMovie = [&](const QString &file) -> QMovie* {
        const QString path = base + "/" + file;
        if (!QFileInfo::exists(path)) {
            qWarning() << "Thiếu file:" << path;
            return nullptr;
        }
        auto *m = new QMovie(path, QByteArray(), this);
        m->setScaledSize(currentPetQSize());
        connect(m, &QMovie::frameChanged, this, [this, m]() {
            if (m_currentMovie == m)
                refreshCurrentMovieFrame();
        });
        return m;
    };
    auto makePix = [&](const QString &file) -> QPixmap* {
        const QString path = base + "/" + file;
        if (!QFileInfo::exists(path)) {
            qWarning() << "Thiếu file:" << path;
            return nullptr;
        }
        return new QPixmap(path);
    };

    m_idleMovie       = makeMovie("idle.gif");
    m_thinkingMovie   = makeMovie("thinking.gif");
    m_talkingMovie    = makeMovie("talking.gif");
    m_eatingMovie     = makeMovie("eating.gif");
    m_happyPix        = makePix  ("happy.png");
    m_sleepingPix     = makePix  ("sleeping.png");
    m_errorPix        = makePix  ("error.png");
    m_idlePixFallback = makePix  ("idle.png");
}

QPixmap *CharacterWidget::costumePixmap(const QString &id)
{
    if (id.isEmpty())
        return nullptr;

    auto it = m_costumePixmaps.find(id);
    if (it != m_costumePixmaps.end())
        return it.value();

    const QStringList candidates = {
        QApplication::applicationDirPath() + "/assets/costumes",
        QApplication::applicationDirPath() + "/../assets/costumes",
        QApplication::applicationDirPath() + "/../../assets/costumes",
    };
    QString base;
    for (const auto &p : candidates) {
        if (QFileInfo::exists(p)) { base = p; break; }
    }
    if (base.isEmpty()) {
        qWarning() << "Không tìm thấy thư mục assets/costumes!";
        m_costumePixmaps.insert(id, nullptr);
        return nullptr;
    }

    const QString path = base + "/" + id + ".png";
    if (!QFileInfo::exists(path)) {
        qWarning() << "Thiếu file trang phục:" << path;
        m_costumePixmaps.insert(id, nullptr);
        return nullptr;
    }

    auto *pix = new QPixmap(path);
    m_costumePixmaps.insert(id, pix);
    return pix;
}

void CharacterWidget::setCostume(const QString &id)
{
    if (m_costume == id)
        return;

    if (!id.isEmpty() && !costumePixmap(id)) {
        qWarning() << "Không thể đổi sang trang phục:" << id;
        return;
    }

    m_costume = id;
    PetState::instance().setCostume(m_costume);
    PetState::instance().save();
    setState(m_state);
}

void CharacterWidget::setState(CharacterState state)
{
    m_state = state;

    if (m_currentMovie) {
        m_currentMovie->stop();
        m_currentMovie = nullptr;
    }

    if (!m_costume.isEmpty()) {
        if (QPixmap *cpix = costumePixmap(m_costume)) {
            m_imageLabel->setPixmap(orientPixmap(*cpix));
        }
        m_revertTimer->stop();
        switch (state) {
        case CharacterState::Talking: m_revertTimer->start(kTalkingMs); break;
        case CharacterState::Happy:   m_revertTimer->start(kHappyMs);   break;
        case CharacterState::Error:   m_revertTimer->start(kErrorMs);   break;
        default: break;
        }
        if (state != CharacterState::Sleeping) {
            m_sleepTimer->start(kSleepMs);
        }
        return;
    }

    auto showMovie = [&](QMovie *mv) {
        if (mv) {
            m_currentMovie = mv;
            mv->setScaledSize(currentPetQSize());
            m_imageLabel->setMovie(nullptr);
            mv->start();
            refreshCurrentMovieFrame();
        } else if (m_idlePixFallback) {
            m_imageLabel->setPixmap(orientPixmap(*m_idlePixFallback));
        }
    };
    auto showPix = [&](QPixmap *pix) {
        if (pix) {
            m_imageLabel->setPixmap(orientPixmap(*pix));
        } else if (m_idlePixFallback) {
            m_imageLabel->setPixmap(orientPixmap(*m_idlePixFallback));
        }
    };

    switch (state) {
    case CharacterState::Idle:     showMovie(m_idleMovie);     break;
    case CharacterState::Thinking: showMovie(m_thinkingMovie); break;
    case CharacterState::Talking:  showMovie(m_talkingMovie);  break;
    case CharacterState::Eating:   showMovie(m_eatingMovie);   break;
    case CharacterState::Happy:    showPix  (m_happyPix);      break;
    case CharacterState::Sleeping: showPix  (m_sleepingPix);   break;
    case CharacterState::Error:    showPix  (m_errorPix);      break;
    }

    m_revertTimer->stop();
    switch (state) {
    case CharacterState::Talking: m_revertTimer->start(kTalkingMs); break;
    case CharacterState::Happy:   m_revertTimer->start(kHappyMs);   break;
    case CharacterState::Error:   m_revertTimer->start(kErrorMs);   break;
    default: break;
    }
    if (state != CharacterState::Sleeping) {
        m_sleepTimer->start(kSleepMs);
    }
}

void CharacterWidget::playEatEffect(const QString &text)
{
    if (!m_eatEffectLabel || !m_eatEffectTimer)
        return;

    const QString labelText = text.trimmed().isEmpty() ? QString("yum!") : text.trimmed();
    m_eatEffectStep = 0;
    m_eatEffectLabel->setStyleSheet(
        "QLabel {"
        " background-color: rgba(30, 30, 46, 190);"
        " color: #ffe066;"
        " border: 1px solid rgba(255, 224, 102, 170);"
        " border-radius: 10px;"
        " padding: 3px 7px;"
        " font-weight: bold;"
        " font-size: 11px;"
        "}");
    m_eatEffectLabel->setText(labelText);
    m_eatEffectLabel->adjustSize();

    const int x = (width() - m_eatEffectLabel->width()) / 2;
    const int y = qMax(4, height() / 4);
    m_eatEffectLabel->move(x, y);
    m_eatEffectLabel->show();
    m_eatEffectLabel->raise();
    m_eatEffectTimer->start();
}

void CharacterWidget::onEatEffectTick()
{
    if (!m_eatEffectLabel || !m_eatEffectLabel->isVisible())
        return;

    ++m_eatEffectStep;
    m_eatEffectLabel->move(m_eatEffectLabel->x(), m_eatEffectLabel->y() - 3);

    const int fade = qMax(0, 190 - (m_eatEffectStep * 12));
    m_eatEffectLabel->setStyleSheet(QString(
        "QLabel {"
        " background-color: rgba(30, 30, 46, %1);"
        " color: #ffe066;"
        " border: 1px solid rgba(255, 224, 102, %2);"
        " border-radius: 10px;"
        " padding: 3px 7px;"
        " font-weight: bold;"
        " font-size: 11px;"
        "}").arg(fade).arg(qMax(0, fade - 20)));

    if (m_eatEffectStep >= kEatEffectTicks) {
        m_eatEffectTimer->stop();
        m_eatEffectLabel->hide();
        m_eatEffectLabel->setStyleSheet(
            "QLabel {"
            " background-color: rgba(30, 30, 46, 190);"
            " color: #ffe066;"
            " border: 1px solid rgba(255, 224, 102, 170);"
            " border-radius: 10px;"
            " padding: 3px 7px;"
            " font-weight: bold;"
            " font-size: 11px;"
            "}");
    }
}

void CharacterWidget::resetRoamDirection()
{
    auto *rng = QRandomGenerator::global();
    int dx = 0;
    int dy = 0;
    while (dx == 0 && dy == 0) {
        dx = rng->bounded(-1, 2);
        dy = rng->bounded(-1, 2);
    }

    m_roamVelocity = QPoint(dx, dy);
    setFacingFromVelocity(m_roamVelocity);
    m_roamTicksRemaining = rng->bounded(kRoamMinTicks, kRoamMaxTicks + 1);
}

void CharacterWidget::onRoamTick()
{
    if (m_state != CharacterState::Idle || m_mouseHeld || m_dragging ||
        (m_chatWidget && m_chatWidget->isVisible())) {
        return;
    }

    if (--m_roamTicksRemaining <= 0)
        resetRoamDirection();

    QScreen *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry().adjusted(
        kRoamMargin, kRoamMargin, -kRoamMargin, -kRoamMargin);

    QPoint next = pos() + m_roamVelocity;
    bool bounced = false;

    if (next.x() < available.left()) {
        next.setX(available.left());
        m_roamVelocity.setX(qAbs(m_roamVelocity.x()));
        setFacingFromVelocity(m_roamVelocity);
        bounced = true;
    } else if (next.x() + width() > available.right()) {
        next.setX(available.right() - width());
        m_roamVelocity.setX(-qAbs(m_roamVelocity.x()));
        setFacingFromVelocity(m_roamVelocity);
        bounced = true;
    }

    if (next.y() < available.top()) {
        next.setY(available.top());
        m_roamVelocity.setY(qAbs(m_roamVelocity.y()));
        bounced = true;
    } else if (next.y() + height() > available.bottom()) {
        next.setY(available.bottom() - height());
        m_roamVelocity.setY(-qAbs(m_roamVelocity.y()));
        bounced = true;
    }

    move(next);
    if (bounced)
        m_roamTicksRemaining = 0;
}

void CharacterWidget::applyPetSize(int newSize)
{
    const int clampedSize = std::clamp(newSize, SizeCfg::kMin, SizeCfg::kMax);
    if (m_petSize == clampedSize)
        return;

    const QPoint oldCenter = geometry().center();
    m_petSize = clampedSize;
    resize(m_petSize, m_petSize);
    move(oldCenter - QPoint(m_petSize / 2, m_petSize / 2));
    setState(m_state);
}

void CharacterWidget::onAutoRevertToIdle()
{
    if (m_state == CharacterState::Talking) {
        setState(CharacterState::Happy);
    } else {
        setState(CharacterState::Idle);
    }
}

void CharacterWidget::onSleepTimeout()
{
    if (m_state == CharacterState::Idle) {
        setState(CharacterState::Sleeping);
    }
}

void CharacterWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos   = event->globalPosition().toPoint();
        m_windowStartPos = frameGeometry().topLeft();
        m_dragging = false;
        m_mouseHeld = true;

        if (m_state == CharacterState::Sleeping) {
            setState(CharacterState::Idle);
        }
        event->accept();
    }
}

void CharacterWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) return;
    const QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
    if (!m_dragging && delta.manhattanLength() > kClickThreshold) {
        m_dragging = true;
    }
    if (m_dragging) {
        move(m_windowStartPos + delta);
    }
    event->accept();
}

void CharacterWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_dragging) {
        toggleChat();
    }
    m_dragging = false;
    m_mouseHeld = false;
    event->accept();
}

void CharacterWidget::toggleChat()
{
    if (!m_chatWidget) return;
    if (m_chatWidget->isVisible()) {
        m_chatWidget->hide();
    } else {
        m_chatWidget->showNearCharacter(frameGeometry().topLeft());
    }
}

void CharacterWidget::closeEvent(QCloseEvent *event)
{
    PetState::instance().save();
    QWidget::closeEvent(event);
}

void CharacterWidget::onHungerThreshold(int threshold)
{
    // Không popup khi pet đang ăn
    if (m_state == CharacterState::Eating) return;

    QString title, body;
    if (threshold >= 70) {
        title = "TrashPet đói lắm rồi 🥺🥺🥺";
        body  = "Mình đói lả rồi bạn ơi... Thùng rác có gì không?";
    } else {
        title = "TrashPet hơi đói rồi 🥺";
        body  = "Bụng mình đang sôi nhẹ rồi... có rác nào không?";
    }

    QMessageBox mb(this);
    mb.setWindowTitle(title);
    mb.setText(body);
    mb.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    mb.button(QMessageBox::Ok)->setText("Cho ăn ngay!");
    mb.button(QMessageBox::Cancel)->setText("Để lát");

    if (mb.exec() == QMessageBox::Ok && m_chatWidget) {
        m_chatWidget->showNearCharacter(frameGeometry().topLeft());
        m_chatWidget->triggerEatTrash();
    }
}

namespace {
QString formatBytes(quint64 bytes) {
    if (bytes >= 1024ULL * 1024 * 1024)
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
}
}

void CharacterWidget::onDiskWarning(QString driveLetter, quint64 freeBytes, quint64 totalBytes)
{
    if (m_state == CharacterState::Eating) return;

    setState(CharacterState::Error);
    if (m_chatWidget) {
        m_chatWidget->showNearCharacter(frameGeometry().topLeft());
        m_chatWidget->appendMessage("Bot",
            QString("⚠️ Ổ %1 sắp đầy rồi! Còn %2 trống / %3. Cho mình ăn rác đi! 🗑️")
                .arg(driveLetter, formatBytes(freeBytes), formatBytes(totalBytes)));
    }
}

void CharacterWidget::onRamWarning(double usedPercent)
{
    if (m_state == CharacterState::Eating) return;

    setState(CharacterState::Error);
    if (m_chatWidget) {
        m_chatWidget->showNearCharacter(frameGeometry().topLeft());
        m_chatWidget->appendMessage("Bot",
            QString("⚠️ RAM đang dùng %1%% rồi, hơi nặng máy đó! Cho mình ăn rác đi! 🗑️")
                .arg(usedPercent, 0, 'f', 0));
    }
}

void CharacterWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *chatAction     = menu.addAction("💬 Mở chat");
    QAction *eatAction      = menu.addAction("🗑️ Ăn rác");
    menu.addSeparator();

    // Trang phục: tính năng đã tắt (sprite chất lượng chưa đạt) — giữ code lại để bật lại sau nếu cần.
    QMap<QAction*, QString> costumeActionIds;

    QMenu *aiMenu = menu.addMenu("🤖 Đổi AI");
    auto *aiGroup = new QActionGroup(aiMenu);
    aiGroup->setExclusive(true);
    QMap<QAction*, QString> aiActionIds;
    const QString currentAi = m_chatWidget ? m_chatWidget->currentAiProvider() : "gemini";
    const QVector<QPair<QString, QString>> aiOptions = {
        { "gemini", "🟦 Gemini" },
        { "openai", "🟩 ChatGPT" },
        { "claude", "🟧 Claude" },
    };
    for (const auto &opt : aiOptions) {
        QAction *act = aiMenu->addAction(opt.second);
        act->setCheckable(true);
        act->setChecked(opt.first == currentAi);
        aiGroup->addAction(act);
        aiActionIds.insert(act, opt.first);
    }

    menu.addSeparator();
    QAction *healthAction   = menu.addAction("💻 Tình trạng máy");
    QAction *settingsAction = menu.addAction("⚙️ Cài đặt");
    menu.addSeparator();
    QAction *quitAction     = menu.addAction("❌ Thoát");

    QAction *selected = menu.exec(event->globalPos());
    if (selected == quitAction) {
        QApplication::quit();
    } else if (selected == chatAction) {
        toggleChat();
    } else if (selected == eatAction) {
        if (m_chatWidget) {
            m_chatWidget->showNearCharacter(frameGeometry().topLeft());
            m_chatWidget->triggerEatTrash();
        }
    } else if (selected == healthAction) {
        HealthDialog dlg(m_systemMonitor, this);
        dlg.exec();
    } else if (selected == settingsAction) {
        SetupDialog dlg(this);
        ConfigManager &cfg = ConfigManager::instance();
        dlg.setInitialProvider(cfg.aiProvider());
        if (cfg.aiProvider() == "openai")      dlg.setInitialApiKey(cfg.openaiApiKey());
        else if (cfg.aiProvider() == "claude") dlg.setInitialApiKey(cfg.claudeApiKey());
        else                                    dlg.setInitialApiKey(cfg.apiKey());

        if (dlg.exec() == QDialog::Accepted) {
            cfg.saveProviderApiKey(cfg.configFilePath(), dlg.selectedProvider(), dlg.apiKey());
            if (m_chatWidget) m_chatWidget->setAiProvider(dlg.selectedProvider());
        }
    } else if (costumeActionIds.contains(selected)) {
        setCostume(costumeActionIds.value(selected));
    } else if (aiActionIds.contains(selected)) {
        if (m_chatWidget) m_chatWidget->setAiProvider(aiActionIds.value(selected));
    }
}
