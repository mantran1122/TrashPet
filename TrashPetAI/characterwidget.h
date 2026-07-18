#ifndef CHARACTERWIDGET_H
#define CHARACTERWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QMap>
#include "characterstate.h"

class ChatWidget;
class HungerManager;
class SystemMonitor;
class QMovie;
class QLabel;
class QTimer;
class QPixmap;

class CharacterWidget : public QWidget {
    Q_OBJECT
public:
    explicit CharacterWidget(QWidget *parent = nullptr);
    ~CharacterWidget();

    void setState(CharacterState state);
    void playEatEffect(const QString &text = QString());
    CharacterState currentState() const { return m_state; }

    void setCostume(const QString &id);
    QString currentCostume() const { return m_costume; }

    HungerManager *hungerManager() const { return m_hungerManager; }
    SystemMonitor *systemMonitor() const { return m_systemMonitor; }
    int petSize() const { return m_petSize; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAutoRevertToIdle();
    void onSleepTimeout();
    void onHungerThreshold(int threshold);
    void applyPetSize(int newSize);
    void onEatEffectTick();
    void onRoamTick();
    void onDiskWarning(QString driveLetter, quint64 freeBytes, quint64 totalBytes);
    void onRamWarning(double usedPercent);

private:
    void loadAssets();
    void toggleChat();
    QSize currentPetQSize() const;
    QPixmap orientPixmap(const QPixmap &source) const;
    void refreshCurrentMovieFrame();
    void setFacingFromVelocity(const QPoint &velocity);
    void resetRoamDirection();
    QPixmap *costumePixmap(const QString &id);

    // UI
    QLabel  *m_imageLabel  = nullptr;
    QLabel  *m_eatEffectLabel = nullptr;
    QMovie  *m_currentMovie = nullptr;   // không own, chỉ trỏ tới movie hiện hành

    // Cache movie/pixmap theo state để khỏi load lại
    QMovie  *m_idleMovie    = nullptr;
    QMovie  *m_thinkingMovie= nullptr;
    QMovie  *m_talkingMovie = nullptr;
    QMovie  *m_eatingMovie  = nullptr;
    QPixmap *m_happyPix     = nullptr;
    QPixmap *m_sleepingPix  = nullptr;
    QPixmap *m_errorPix     = nullptr;
    QPixmap *m_idlePixFallback = nullptr;
    int      m_petSize = 160;

    // Trang phục
    QString  m_costume;   // "" = mặc định (slime)
    QMap<QString, QPixmap*> m_costumePixmaps;  // cache, lazy load

    // State
    CharacterState m_state = CharacterState::Idle;
    QTimer *m_revertTimer = nullptr;
    QTimer *m_sleepTimer  = nullptr;
    QTimer *m_eatEffectTimer = nullptr;
    QTimer *m_roamTimer = nullptr;
    int     m_eatEffectStep = 0;
    int     m_roamTicksRemaining = 0;
    QPoint  m_roamVelocity;
    bool    m_facingLeft = true;  // asset gốc nhìn sang trái

    // Drag/click logic
    QPoint m_dragStartPos;
    QPoint m_windowStartPos;
    bool   m_dragging = false;
    bool   m_mouseHeld = false;

    // Chat
    ChatWidget    *m_chatWidget    = nullptr;

    // Hunger system
    HungerManager *m_hungerManager = nullptr;

    // Health monitor (disk/RAM)
    SystemMonitor *m_systemMonitor = nullptr;
};

#endif // CHARACTERWIDGET_H
