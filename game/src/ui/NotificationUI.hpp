#pragma once

#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <memory>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {

/**
 * @brief Notification type/severity
 */
enum class NotificationType {
    Info,
    Success,
    Warning,
    Error,
    Achievement,
    Quest,
    System
};

/**
 * @brief Notification position
 */
enum class NotificationPosition {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Center
};

/**
 * @brief Toast notification data
 */
struct ToastNotification {
    std::string id;
    std::string title;
    std::string message;
    std::string iconPath;
    NotificationType type = NotificationType::Info;
    float duration = 3.0f;
    bool dismissable = true;
    bool playSound = true;
    std::string soundPath;
    std::function<void()> onClick;
};

/**
 * @brief Achievement popup data
 */
struct AchievementPopup {
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;
    std::string rarity; // "common", "rare", "epic", "legendary"
    int points = 0;
    bool unlocked = true;
};

/**
 * @brief Quest update data
 */
struct QuestUpdate {
    std::string questId;
    std::string questName;
    std::string updateType; // "started", "progress", "completed", "failed"
    std::string description;
    std::string iconPath;
};

/**
 * @brief System message data
 */
struct SystemMessage {
    std::string message;
    NotificationType type = NotificationType::System;
    bool persistent = false;
    std::string actionText;
    std::function<void()> onAction;
};

/**
 * @brief Notification configuration
 */
struct NotificationConfig {
    NotificationPosition position = NotificationPosition::TopRight;
    int maxVisible = 5;
    float defaultDuration = 3.0f;
    bool stackNotifications = true;
    bool enableSounds = true;
    float globalVolume = 1.0f;
    std::string enterAnimation = "slideInRight";
    std::string exitAnimation = "fadeOut";
};

/**
 * @brief Notification UI System
 *
 * Toast notifications, achievement popups, quest updates,
 * and system messages.
 */
class NotificationUI {
public:
    NotificationUI();
    ~NotificationUI();

    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);
    void Shutdown();
    void Update(float deltaTime);

    // Configuration
    void SetConfig(const NotificationConfig& config);
    const NotificationConfig& GetConfig() const { return m_config; }
    void SetPosition(NotificationPosition position);
    void SetMaxVisible(int max);
    void SetSoundsEnabled(bool enabled);

    // Toast Notifications
    std::string ShowToast(const ToastNotification& notification);
    std::string ShowToast(const std::string& message, NotificationType type = NotificationType::Info, float duration = 3.0f);
    std::string ShowToast(const std::string& title, const std::string& message, NotificationType type = NotificationType::Info);
    void DismissToast(const std::string& notificationId);
    void DismissAllToasts();

    // Quick Notifications
    void ShowInfo(const std::string& message, float duration = 3.0f);
    void ShowSuccess(const std::string& message, float duration = 3.0f);
    void ShowWarning(const std::string& message, float duration = 3.0f);
    void ShowError(const std::string& message, float duration = 5.0f);

    // Achievement Popups
    void ShowAchievement(const AchievementPopup& achievement);
    void ShowAchievement(const std::string& name, const std::string& description, const std::string& iconPath = "");

    // Quest Updates
    void ShowQuestUpdate(const QuestUpdate& update);
    void ShowQuestStarted(const std::string& questName, const std::string& description = "");
    void ShowQuestProgress(const std::string& questName, const std::string& progress);
    void ShowQuestCompleted(const std::string& questName);
    void ShowQuestFailed(const std::string& questName);

    // System Messages
    void ShowSystemMessage(const SystemMessage& message);
    void ShowSystemMessage(const std::string& message, bool persistent = false);
    void ShowSystemMessageWithAction(const std::string& message, const std::string& actionText, std::function<void()> onAction);
    void DismissSystemMessage();

    // Banner Notifications (full-width)
    void ShowBanner(const std::string& message, NotificationType type, float duration = 5.0f);
    void DismissBanner();

    // Confirmation Dialogs
    void ShowConfirmation(const std::string& title, const std::string& message,
                         std::function<void(bool)> callback,
                         const std::string& confirmText = "Confirm",
                         const std::string& cancelText = "Cancel");

    // Progress Notifications
    std::string ShowProgress(const std::string& title, const std::string& message);
    void UpdateProgress(const std::string& progressId, float percent, const std::string& status = "");
    void CompleteProgress(const std::string& progressId, bool success = true);

    // Queue Management
    void ClearQueue();
    int GetQueueSize() const { return static_cast<int>(m_notificationQueue.size()); }
    int GetActiveCount() const { return static_cast<int>(m_activeNotifications.size()); }

    // History
    void EnableHistory(bool enabled);
    void ClearHistory();
    std::vector<ToastNotification> GetHistory() const { return m_history; }

    // Callbacks
    void SetOnNotificationShow(std::function<void(const std::string&)> callback);
    void SetOnNotificationDismiss(std::function<void(const std::string&)> callback);

private:
    void ProcessQueue();
    void AddToActive(const ToastNotification& notification);
    void RemoveFromActive(const std::string& id);
    void UpdatePositions();
    void PlayNotificationSound(const ToastNotification& notification);
    std::string GenerateId();
    void SetupEventHandlers();

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    Engine::UI::UIWindow* m_notificationContainer = nullptr;
    Engine::UI::UIWindow* m_achievementWindow = nullptr;
    Engine::UI::UIWindow* m_bannerWindow = nullptr;

    NotificationConfig m_config;

    struct ActiveNotification {
        ToastNotification data;
        float timeRemaining;
        bool dismissing = false;
    };

    std::vector<ActiveNotification> m_activeNotifications;
    std::queue<ToastNotification> m_notificationQueue;
    std::vector<ToastNotification> m_history;

    bool m_historyEnabled = true;
    int m_nextId = 1;

    // Current banner
    bool m_bannerActive = false;
    float m_bannerTimeRemaining = 0;

    // Callbacks
    std::function<void(const std::string&)> m_onShow;
    std::function<void(const std::string&)> m_onDismiss;
};

} // namespace UI
} // namespace Game
