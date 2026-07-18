#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include "recyclebinmanager.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QTimer;
class AiClient;
class CharacterWidget;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    void showNearCharacter(const QPoint &characterPos);
    void setCharacterWidget(CharacterWidget *cw);
    void triggerEatTrash();

    // Multi-provider AI: "gemini" | "openai" | "claude"
    void setAiProvider(const QString &id);
    QString currentAiProvider() const { return m_aiProvider; }

    void appendMessage(const QString &sender, const QString &text);

private slots:
    void onSendClicked();
    void onAiResponse(const QString &text);
    void onAiError(const QString &errorMessage);
    void onEatTick();
    void onHungerChanged(int hunger);

private:
    void finishEating();

    // UI
    QTextEdit    *m_chatArea;
    QLineEdit    *m_inputBox;
    QPushButton  *m_sendButton;
    QPushButton  *m_eatButton;
    QProgressBar *m_hungerBar   = nullptr;
    QLabel       *m_hungerLabel = nullptr;

    AiClient        *m_geminiClient = nullptr;
    AiClient        *m_openaiClient = nullptr;
    AiClient        *m_claudeClient = nullptr;
    QString          m_aiProvider = "gemini";
    CharacterWidget *m_character = nullptr;

    // Eat queue
    QTimer                          *m_eatTimer    = nullptr;
    QVector<RecycleBinManager::Item> m_eatQueue;
    QStringList                      m_skippedNames;
    QStringList                      m_failedNames;
    int                              m_eatenCount        = 0;
    quint64                          m_deletedBytes      = 0;
    int                              m_hungerConsumedMb  = 0;
};

#endif // CHATWIDGET_H
