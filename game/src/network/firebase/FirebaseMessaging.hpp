#pragma once

#include "FirebaseCore.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <queue>

namespace Network {
namespace Firebase {

// Message types
enum class MessageType {
    PlayerToPlayer,    // Direct message between players
    TeamChat,          // Team-wide message
    GlobalChat,        // Global chat channel
    System,            // System notification
    Whisper,           // Private whisper
    GroupChat,         // Custom group/guild chat
    MatchChat,         // Match-specific chat
    LobbyChat          // Lobby chat
};

// Message priority for push notifications
enum class MessagePriority {
    Low,
    Normal,
    High,
    Critical
};

// Moderation action types
enum class ModerationAction {
    None,
    Filtered,      // Profanity filtered
    Blocked,       // Sender is blocked
    Muted,         // Sender is muted
    Flagged,       // Flagged for review
    Deleted        // Message deleted by moderator
};

// Chat message
struct ChatMessage {
    std::string messageId;
    std::string senderId;
    std::string senderName;
    std::string recipientId;      // For direct messages
    std::string channelId;        // For channel messages
    MessageType type = MessageType::PlayerToPlayer;

    std::string content;
    std::string filteredContent;  // After moderation filter
    bool isFiltered = false;

    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point editedAt;
    bool isEdited = false;
    bool isDeleted = false;

    MessagePriority priority = MessagePriority::Normal;
    ModerationAction modAction = ModerationAction::None;

    // Rich content
    std::string attachmentUrl;
    std::string attachmentType;   // image, link, etc.
    std::unordered_map<std::string, std::string> metadata;

    // Reactions
    std::unordered_map<std::string, std::vector<std::string>> reactions;  // emoji -> user IDs
};

// Chat channel
struct ChatChannel {
    std::string channelId;
    std::string name;
    std::string description;
    MessageType type = MessageType::GlobalChat;

    std::string ownerId;
    std::vector<std::string> memberIds;
    std::vector<std::string> moderatorIds;

    bool isPublic = true;
    bool isReadOnly = false;
    int maxMembers = 100;

    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastMessageAt;
    int messageCount = 0;

    std::unordered_map<std::string, std::string> settings;
};

// Conversation (for direct messages)
struct Conversation {
    std::string conversationId;
    std::vector<std::string> participantIds;
    std::string lastMessageId;
    std::chrono::system_clock::time_point lastMessageAt;
    std::unordered_map<std::string, int> unreadCounts;  // userId -> count
};

// Push notification data
struct PushNotification {
    std::string title;
    std::string body;
    std::string imageUrl;
    std::string clickAction;
    std::unordered_map<std::string, std::string> data;
    MessagePriority priority = MessagePriority::Normal;
};

// Mute/block entry
struct MuteEntry {
    std::string oderId;
    std::chrono::system_clock::time_point mutedAt;
    std::chrono::system_clock::time_point expiresAt;  // Zero = permanent
    std::string reason;
    bool isPermanent = false;
};

// Profanity filter result
struct FilterResult {
    std::string original;
    std::string filtered;
    bool wasFiltered = false;
    std::vector<std::string> flaggedWords;
    float toxicityScore = 0.0f;
};

// Callbacks
using MessageCallback = std::function<void(const ChatMessage&)>;
using MessageListCallback = std::function<void(const std::vector<ChatMessage>&, const FirebaseError&)>;
using ChannelCallback = std::function<void(const ChatChannel&, const FirebaseError&)>;
using ConversationCallback = std::function<void(const Conversation&, const FirebaseError&)>;
using NotificationCallback = std::function<void(const PushNotification&)>;

/**
 * FirebaseMessaging - In-game messaging system
 *
 * Features:
 * - Player-to-player messages
 * - Team and global chat channels
 * - Message history
 * - Push notifications
 * - Profanity filtering
 * - Mute/block system
 */
class FirebaseMessaging {
public:
    static FirebaseMessaging& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Direct messaging
    void sendMessage(const std::string& recipientId, const std::string& content,
                     std::function<void(const ChatMessage&, const FirebaseError&)> callback = nullptr);
    void sendMessage(const ChatMessage& message,
                     std::function<void(const ChatMessage&, const FirebaseError&)> callback = nullptr);

    // Channel messaging
    void sendToChannel(const std::string& channelId, const std::string& content,
                       std::function<void(const ChatMessage&, const FirebaseError&)> callback = nullptr);
    void sendToTeam(const std::string& content);
    void sendToMatch(const std::string& content);
    void sendToLobby(const std::string& content);

    // Message management
    void editMessage(const std::string& messageId, const std::string& newContent,
                     std::function<void(const FirebaseError&)> callback = nullptr);
    void deleteMessage(const std::string& messageId,
                       std::function<void(const FirebaseError&)> callback = nullptr);
    void addReaction(const std::string& messageId, const std::string& emoji);
    void removeReaction(const std::string& messageId, const std::string& emoji);

    // Message history
    void getMessageHistory(const std::string& channelOrConversationId, int count, int offset,
                           MessageListCallback callback);
    void getMessagesBefore(const std::string& channelId, const std::string& beforeMessageId,
                           int count, MessageListCallback callback);
    void getMessagesAfter(const std::string& channelId, const std::string& afterMessageId,
                          int count, MessageListCallback callback);
    void searchMessages(const std::string& query, int count, MessageListCallback callback);

    // Conversations
    void getConversations(std::function<void(const std::vector<Conversation>&, const FirebaseError&)> callback);
    void getConversation(const std::string& oderId, ConversationCallback callback);
    void markAsRead(const std::string& conversationId);
    int getUnreadCount() const;
    int getUnreadCount(const std::string& conversationOrChannelId) const;

    // Channel management
    void createChannel(const std::string& name, MessageType type, bool isPublic,
                       ChannelCallback callback);
    void deleteChannel(const std::string& channelId,
                       std::function<void(const FirebaseError&)> callback);
    void joinChannel(const std::string& channelId,
                     std::function<void(const FirebaseError&)> callback);
    void leaveChannel(const std::string& channelId);
    void getChannel(const std::string& channelId, ChannelCallback callback);
    void listChannels(MessageType type,
                      std::function<void(const std::vector<ChatChannel>&, const FirebaseError&)> callback);
    void listMyChannels(std::function<void(const std::vector<ChatChannel>&, const FirebaseError&)> callback);

    // Channel moderation
    void addModerator(const std::string& channelId, const std::string& oderId);
    void removeModerator(const std::string& channelId, const std::string& oderId);
    void kickFromChannel(const std::string& channelId, const std::string& oderId);
    void banFromChannel(const std::string& channelId, const std::string& oderId,
                        std::chrono::seconds duration = std::chrono::seconds(0));

    // Team/Match context
    void setTeamId(const std::string& teamId) { m_currentTeamId = teamId; }
    void setMatchId(const std::string& matchId) { m_currentMatchId = matchId; }
    void setLobbyId(const std::string& lobbyId) { m_currentLobbyId = lobbyId; }

    // Push notifications
    void registerForPushNotifications(const std::string& deviceToken);
    void unregisterFromPushNotifications();
    void sendPushNotification(const std::string& recipientId, const PushNotification& notification);
    void onPushNotification(NotificationCallback callback);
    void setNotificationsEnabled(bool enabled) { m_notificationsEnabled = enabled; }
    bool areNotificationsEnabled() const { return m_notificationsEnabled; }

    // Mute/Block
    void mutePlayer(const std::string& oderId, std::chrono::seconds duration = std::chrono::seconds(0));
    void unmutePlayer(const std::string& oderId);
    void blockPlayer(const std::string& oderId);
    void unblockPlayer(const std::string& oderId);
    bool isPlayerMuted(const std::string& oderId) const;
    bool isPlayerBlocked(const std::string& oderId) const;
    std::vector<MuteEntry> getMutedPlayers() const;
    std::vector<std::string> getBlockedPlayers() const;

    // Profanity filter
    void enableProfanityFilter(bool enabled) { m_profanityFilterEnabled = enabled; }
    bool isProfanityFilterEnabled() const { return m_profanityFilterEnabled; }
    FilterResult filterMessage(const std::string& content);
    void addCustomFilter(const std::string& word);
    void removeCustomFilter(const std::string& word);

    // Message listeners
    void onMessage(MessageCallback callback);
    void onMessageInChannel(const std::string& channelId, MessageCallback callback);
    void removeMessageListener(const std::string& channelId = "");

    // Typing indicators
    void setTyping(const std::string& channelOrConversationId, bool isTyping);
    void onTypingIndicator(std::function<void(const std::string& oderId, const std::string& channelId, bool isTyping)> callback);

private:
    FirebaseMessaging();
    ~FirebaseMessaging();
    FirebaseMessaging(const FirebaseMessaging&) = delete;
    FirebaseMessaging& operator=(const FirebaseMessaging&) = delete;

    // Internal messaging
    void uploadMessage(const ChatMessage& message,
                       std::function<void(const ChatMessage&, const FirebaseError&)> callback);
    void downloadMessages(const std::string& path, int count, int offset,
                          MessageListCallback callback);

    // Notification handling
    void handleIncomingMessage(const ChatMessage& message);
    void createLocalNotification(const ChatMessage& message);

    // Moderation
    bool shouldFilterMessage(const ChatMessage& message) const;
    ChatMessage applyFilters(const ChatMessage& message);

    // Polling for messages
    void pollForNewMessages();
    void processMessageQueue();

    // Serialization
    std::string serializeMessage(const ChatMessage& message);
    ChatMessage deserializeMessage(const std::string& json);
    std::string serializeChannel(const ChatChannel& channel);
    ChatChannel deserializeChannel(const std::string& json);

    // ID generation
    std::string generateMessageId();
    std::string generateChannelId();
    std::string getConversationId(const std::string& oderId1, const std::string& oderId2);

private:
    bool m_initialized = false;

    // Current context
    std::string m_currentTeamId;
    std::string m_currentMatchId;
    std::string m_currentLobbyId;

    // Message listeners
    std::vector<MessageCallback> m_globalMessageCallbacks;
    std::unordered_map<std::string, std::vector<MessageCallback>> m_channelMessageCallbacks;
    std::function<void(const std::string&, const std::string&, bool)> m_typingCallback;

    // Push notifications
    bool m_notificationsEnabled = true;
    std::string m_deviceToken;
    std::vector<NotificationCallback> m_notificationCallbacks;

    // Mute/Block
    std::unordered_map<std::string, MuteEntry> m_mutedPlayers;
    std::unordered_set<std::string> m_blockedPlayers;

    // Profanity filter
    bool m_profanityFilterEnabled = true;
    std::unordered_set<std::string> m_profanityList;
    std::unordered_set<std::string> m_customFilters;

    // Message cache
    std::unordered_map<std::string, std::vector<ChatMessage>> m_messageCache;
    std::unordered_map<std::string, int> m_unreadCounts;
    int m_totalUnreadCount = 0;

    // Conversations cache
    std::unordered_map<std::string, Conversation> m_conversations;

    // Channels cache
    std::unordered_map<std::string, ChatChannel> m_channelCache;
    std::vector<std::string> m_joinedChannels;

    // Message queue for offline support
    std::queue<ChatMessage> m_outgoingQueue;

    // Polling
    float m_pollTimer = 0.0f;
    static constexpr float POLL_INTERVAL = 2.0f;

    // Typing indicator timeout
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_typingTimestamps;
    static constexpr float TYPING_TIMEOUT = 5.0f;

    // Rate limiting
    std::chrono::steady_clock::time_point m_lastMessageTime;
    static constexpr float MIN_MESSAGE_INTERVAL = 0.5f;  // 500ms between messages
};

} // namespace Firebase
} // namespace Network
