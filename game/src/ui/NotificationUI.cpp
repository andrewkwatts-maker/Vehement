#include "NotificationUI.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include "engine/ui/runtime/UIAnimation.hpp"

#include <algorithm>

namespace Game {
namespace UI {

NotificationUI::NotificationUI() = default;
NotificationUI::~NotificationUI() { Shutdown(); }

bool NotificationUI::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;
    m_uiManager = uiManager;

    // Create notification container
    m_notificationContainer = m_uiManager->CreateWindow("notifications", "",
                                                        Engine::UI::UILayer::Tooltips);
    if (m_notificationContainer) {
        m_notificationContainer->SetTitleBarVisible(false);
        m_notificationContainer->SetBackgroundColor(Engine::UI::Color(0, 0, 0, 0));
    }

    // Create achievement popup window
    m_achievementWindow = m_uiManager->CreateWindow("achievement_popup", "",
                                                    Engine::UI::UILayer::Popups);
    if (m_achievementWindow) {
        m_achievementWindow->SetTitleBarVisible(false);
        m_achievementWindow->Hide();
    }

    // Create banner window
    m_bannerWindow = m_uiManager->CreateWindow("banner", "",
                                               Engine::UI::UILayer::Popups);
    if (m_bannerWindow) {
        m_bannerWindow->SetTitleBarVisible(false);
        m_bannerWindow->Hide();
    }

    SetupEventHandlers();
    return true;
}

void NotificationUI::Shutdown() {
    if (m_uiManager) {
        if (m_notificationContainer) m_uiManager->CloseWindow("notifications");
        if (m_achievementWindow) m_uiManager->CloseWindow("achievement_popup");
        if (m_bannerWindow) m_uiManager->CloseWindow("banner");
    }
    m_notificationContainer = nullptr;
    m_achievementWindow = nullptr;
    m_bannerWindow = nullptr;
}

void NotificationUI::Update(float deltaTime) {
    // Update active notifications
    for (auto it = m_activeNotifications.begin(); it != m_activeNotifications.end();) {
        it->timeRemaining -= deltaTime;

        if (it->timeRemaining <= 0 && !it->dismissing) {
            it->dismissing = true;
            m_uiManager->GetAnimation().Play(m_config.exitAnimation, "notification-" + it->data.id);
        }

        if (it->timeRemaining <= -0.5f) { // Animation time
            if (m_onDismiss) m_onDismiss(it->data.id);
            it = m_activeNotifications.erase(it);
            UpdatePositions();
        } else {
            ++it;
        }
    }

    // Update banner
    if (m_bannerActive) {
        m_bannerTimeRemaining -= deltaTime;
        if (m_bannerTimeRemaining <= 0) {
            DismissBanner();
        }
    }

    // Process queue
    ProcessQueue();
}

void NotificationUI::SetConfig(const NotificationConfig& config) {
    m_config = config;
}

void NotificationUI::SetPosition(NotificationPosition position) {
    m_config.position = position;
    UpdatePositions();
}

void NotificationUI::SetMaxVisible(int max) {
    m_config.maxVisible = max;
}

void NotificationUI::SetSoundsEnabled(bool enabled) {
    m_config.enableSounds = enabled;
}

std::string NotificationUI::ShowToast(const ToastNotification& notification) {
    ToastNotification notif = notification;
    if (notif.id.empty()) {
        notif.id = GenerateId();
    }

    if (static_cast<int>(m_activeNotifications.size()) < m_config.maxVisible) {
        AddToActive(notif);
    } else if (m_config.stackNotifications) {
        m_notificationQueue.push(notif);
    }

    return notif.id;
}

std::string NotificationUI::ShowToast(const std::string& message, NotificationType type, float duration) {
    ToastNotification notif;
    notif.message = message;
    notif.type = type;
    notif.duration = duration;
    return ShowToast(notif);
}

std::string NotificationUI::ShowToast(const std::string& title, const std::string& message, NotificationType type) {
    ToastNotification notif;
    notif.title = title;
    notif.message = message;
    notif.type = type;
    notif.duration = m_config.defaultDuration;
    return ShowToast(notif);
}

void NotificationUI::DismissToast(const std::string& notificationId) {
    for (auto& notif : m_activeNotifications) {
        if (notif.data.id == notificationId) {
            notif.timeRemaining = 0;
            notif.dismissing = true;
            m_uiManager->GetAnimation().Play(m_config.exitAnimation, "notification-" + notificationId);
            break;
        }
    }
}

void NotificationUI::DismissAllToasts() {
    for (auto& notif : m_activeNotifications) {
        notif.timeRemaining = 0;
        notif.dismissing = true;
    }
    while (!m_notificationQueue.empty()) {
        m_notificationQueue.pop();
    }
}

void NotificationUI::ShowInfo(const std::string& message, float duration) {
    ShowToast(message, NotificationType::Info, duration);
}

void NotificationUI::ShowSuccess(const std::string& message, float duration) {
    ShowToast(message, NotificationType::Success, duration);
}

void NotificationUI::ShowWarning(const std::string& message, float duration) {
    ShowToast(message, NotificationType::Warning, duration);
}

void NotificationUI::ShowError(const std::string& message, float duration) {
    ShowToast(message, NotificationType::Error, duration);
}

void NotificationUI::ShowAchievement(const AchievementPopup& achievement) {
    nlohmann::json args;
    args["id"] = achievement.id;
    args["name"] = achievement.name;
    args["description"] = achievement.description;
    args["icon"] = achievement.iconPath;
    args["rarity"] = achievement.rarity;
    args["points"] = achievement.points;

    m_uiManager->GetBinding().CallJS("Notifications.showAchievement", args);

    if (m_achievementWindow) {
        m_achievementWindow->Show();
        m_achievementWindow->Center();
        m_uiManager->GetAnimation().Play("scaleIn", "achievement_popup");
    }

    // Auto-hide after delay
    // Would set a timer in production
}

void NotificationUI::ShowAchievement(const std::string& name, const std::string& description, const std::string& iconPath) {
    AchievementPopup achievement;
    achievement.name = name;
    achievement.description = description;
    achievement.iconPath = iconPath;
    achievement.rarity = "common";
    ShowAchievement(achievement);
}

void NotificationUI::ShowQuestUpdate(const QuestUpdate& update) {
    ToastNotification notif;
    notif.type = NotificationType::Quest;
    notif.title = update.questName;
    notif.iconPath = update.iconPath;
    notif.duration = 4.0f;

    if (update.updateType == "started") {
        notif.message = "New Quest: " + update.description;
    } else if (update.updateType == "progress") {
        notif.message = update.description;
    } else if (update.updateType == "completed") {
        notif.message = "Quest Completed!";
    } else if (update.updateType == "failed") {
        notif.message = "Quest Failed";
        notif.type = NotificationType::Error;
    }

    ShowToast(notif);
}

void NotificationUI::ShowQuestStarted(const std::string& questName, const std::string& description) {
    QuestUpdate update;
    update.questName = questName;
    update.updateType = "started";
    update.description = description;
    ShowQuestUpdate(update);
}

void NotificationUI::ShowQuestProgress(const std::string& questName, const std::string& progress) {
    QuestUpdate update;
    update.questName = questName;
    update.updateType = "progress";
    update.description = progress;
    ShowQuestUpdate(update);
}

void NotificationUI::ShowQuestCompleted(const std::string& questName) {
    QuestUpdate update;
    update.questName = questName;
    update.updateType = "completed";
    ShowQuestUpdate(update);
}

void NotificationUI::ShowQuestFailed(const std::string& questName) {
    QuestUpdate update;
    update.questName = questName;
    update.updateType = "failed";
    ShowQuestUpdate(update);
}

void NotificationUI::ShowSystemMessage(const SystemMessage& message) {
    nlohmann::json args;
    args["message"] = message.message;
    args["type"] = static_cast<int>(message.type);
    args["persistent"] = message.persistent;
    args["actionText"] = message.actionText;

    m_uiManager->GetBinding().CallJS("Notifications.showSystemMessage", args);
}

void NotificationUI::ShowSystemMessage(const std::string& message, bool persistent) {
    SystemMessage msg;
    msg.message = message;
    msg.persistent = persistent;
    ShowSystemMessage(msg);
}

void NotificationUI::ShowSystemMessageWithAction(const std::string& message, const std::string& actionText, std::function<void()> onAction) {
    SystemMessage msg;
    msg.message = message;
    msg.actionText = actionText;
    msg.onAction = onAction;
    msg.persistent = true;
    ShowSystemMessage(msg);
}

void NotificationUI::DismissSystemMessage() {
    m_uiManager->GetBinding().CallJS("Notifications.dismissSystemMessage", {});
}

void NotificationUI::ShowBanner(const std::string& message, NotificationType type, float duration) {
    nlohmann::json args;
    args["message"] = message;
    args["type"] = static_cast<int>(type);

    m_uiManager->GetBinding().CallJS("Notifications.showBanner", args);

    if (m_bannerWindow) {
        m_bannerWindow->Show();
        m_uiManager->GetAnimation().Play("slideInLeft", "banner");
    }

    m_bannerActive = true;
    m_bannerTimeRemaining = duration;
}

void NotificationUI::DismissBanner() {
    m_bannerActive = false;
    m_uiManager->GetAnimation().Play("fadeOut", "banner");
    if (m_bannerWindow) {
        m_bannerWindow->Hide();
    }
}

void NotificationUI::ShowConfirmation(const std::string& title, const std::string& message,
                                     std::function<void(bool)> callback,
                                     const std::string& confirmText,
                                     const std::string& cancelText) {
    Engine::UI::ModalConfig config;
    config.title = title;
    config.message = message;
    config.buttons = {cancelText, confirmText};
    config.callback = [callback](Engine::UI::ModalResult result, const std::string& data) {
        if (callback) {
            callback(data == "1"); // Second button (confirm) clicked
        }
    };

    m_uiManager->ShowModal(config);
}

std::string NotificationUI::ShowProgress(const std::string& title, const std::string& message) {
    ToastNotification notif;
    notif.id = GenerateId();
    notif.title = title;
    notif.message = message;
    notif.type = NotificationType::Info;
    notif.duration = 999999; // Persistent until manually dismissed
    notif.dismissable = false;

    AddToActive(notif);

    return notif.id;
}

void NotificationUI::UpdateProgress(const std::string& progressId, float percent, const std::string& status) {
    nlohmann::json args;
    args["id"] = progressId;
    args["percent"] = percent;
    args["status"] = status;

    m_uiManager->GetBinding().CallJS("Notifications.updateProgress", args);
}

void NotificationUI::CompleteProgress(const std::string& progressId, bool success) {
    nlohmann::json args;
    args["id"] = progressId;
    args["success"] = success;

    m_uiManager->GetBinding().CallJS("Notifications.completeProgress", args);

    // Dismiss after short delay
    for (auto& notif : m_activeNotifications) {
        if (notif.data.id == progressId) {
            notif.timeRemaining = 1.0f;
            break;
        }
    }
}

void NotificationUI::ClearQueue() {
    while (!m_notificationQueue.empty()) {
        m_notificationQueue.pop();
    }
}

void NotificationUI::EnableHistory(bool enabled) {
    m_historyEnabled = enabled;
}

void NotificationUI::ClearHistory() {
    m_history.clear();
}

void NotificationUI::SetOnNotificationShow(std::function<void(const std::string&)> callback) {
    m_onShow = callback;
}

void NotificationUI::SetOnNotificationDismiss(std::function<void(const std::string&)> callback) {
    m_onDismiss = callback;
}

void NotificationUI::ProcessQueue() {
    while (!m_notificationQueue.empty() &&
           static_cast<int>(m_activeNotifications.size()) < m_config.maxVisible) {
        AddToActive(m_notificationQueue.front());
        m_notificationQueue.pop();
    }
}

void NotificationUI::AddToActive(const ToastNotification& notification) {
    ActiveNotification active;
    active.data = notification;
    active.timeRemaining = notification.duration;
    active.dismissing = false;

    m_activeNotifications.push_back(active);

    // Add to history
    if (m_historyEnabled) {
        m_history.push_back(notification);
        if (m_history.size() > 100) {
            m_history.erase(m_history.begin());
        }
    }

    // Render notification
    nlohmann::json args;
    args["id"] = notification.id;
    args["title"] = notification.title;
    args["message"] = notification.message;
    args["icon"] = notification.iconPath;
    args["type"] = static_cast<int>(notification.type);
    args["dismissable"] = notification.dismissable;

    m_uiManager->GetBinding().CallJS("Notifications.addToast", args);
    m_uiManager->GetAnimation().Play(m_config.enterAnimation, "notification-" + notification.id);

    if (m_config.enableSounds && notification.playSound) {
        PlayNotificationSound(notification);
    }

    if (m_onShow) {
        m_onShow(notification.id);
    }

    UpdatePositions();
}

void NotificationUI::RemoveFromActive(const std::string& id) {
    m_activeNotifications.erase(
        std::remove_if(m_activeNotifications.begin(), m_activeNotifications.end(),
            [&id](const ActiveNotification& n) { return n.data.id == id; }),
        m_activeNotifications.end());
}

void NotificationUI::UpdatePositions() {
    nlohmann::json args;
    args["position"] = static_cast<int>(m_config.position);
    args["count"] = static_cast<int>(m_activeNotifications.size());

    nlohmann::json ids = nlohmann::json::array();
    for (const auto& notif : m_activeNotifications) {
        ids.push_back(notif.data.id);
    }
    args["ids"] = ids;

    m_uiManager->GetBinding().CallJS("Notifications.updatePositions", args);
}

void NotificationUI::PlayNotificationSound(const ToastNotification& notification) {
    std::string soundPath = notification.soundPath;
    if (soundPath.empty()) {
        switch (notification.type) {
            case NotificationType::Success: soundPath = "sounds/ui/success.wav"; break;
            case NotificationType::Warning: soundPath = "sounds/ui/warning.wav"; break;
            case NotificationType::Error: soundPath = "sounds/ui/error.wav"; break;
            case NotificationType::Achievement: soundPath = "sounds/ui/achievement.wav"; break;
            case NotificationType::Quest: soundPath = "sounds/ui/quest.wav"; break;
            default: soundPath = "sounds/ui/notification.wav"; break;
        }
    }

    nlohmann::json args;
    args["path"] = soundPath;
    args["volume"] = m_config.globalVolume;
    m_uiManager->GetBinding().CallJS("Audio.playSound", args);
}

std::string NotificationUI::GenerateId() {
    return "notif_" + std::to_string(m_nextId++);
}

void NotificationUI::SetupEventHandlers() {
    auto& binding = m_uiManager->GetBinding();

    binding.ExposeFunction("Notifications.onDismiss", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            DismissToast(args["id"].get<std::string>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Notifications.onClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            std::string id = args["id"].get<std::string>();
            for (const auto& notif : m_activeNotifications) {
                if (notif.data.id == id && notif.data.onClick) {
                    notif.data.onClick();
                    break;
                }
            }
        }
        return nullptr;
    });
}

} // namespace UI
} // namespace Game
