#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <chrono>

namespace Network {
namespace Replication {

// Chat message types
enum class ChatMessageType {
    Normal,
    System,
    Whisper,
    Team,
    All,
    Emote,
    Command,
    Announcement
};

// Chat message
struct ChatMessage {
    uint64_t messageId;
    uint64_t senderId;
    std::string senderName;
    std::string content;
    ChatMessageType type;
    std::chrono::system_clock::time_point timestamp;
    std::string targetName;  // For whispers
    int team = -1;           // For team messages
    bool isFiltered = false;
    std::string filteredContent;
};

// Emote definition
struct EmoteDefinition {
    std::string code;        // :smile:
    std::string displayText; // For text clients
    std::string imageUrl;    // For graphical clients
};

// Chat command
struct ChatCommand {
    std::string name;
    std::string description;
    std::string usage;
    std::function<void(uint64_t senderId, const std::vector<std::string>& args)> handler;
};

// Chat settings
struct ChatSettings {
    bool filterProfanity = true;
    bool showTimestamps = true;
    bool showSystemMessages = true;
    int maxMessageLength = 500;
    int maxHistorySize = 100;
    float messageRateLimit = 0.5f;  // Min seconds between messages
    std::unordered_set<std::string> mutedPlayers;
    std::unordered_set<std::string> blockedPlayers;
};

// Callbacks
using MessageCallback = std::function<void(const ChatMessage&)>;
using CommandCallback = std::function<void(uint64_t senderId, const std::string& command, const std::vector<std::string>& args)>;

/**
 * ChatSystem - Full chat implementation
 *
 * Features:
 * - Message types (chat, system, whisper, team)
 * - Chat history
 * - Chat commands
 * - Emotes
 * - Mute/block
 */
class ChatSystem {
public:
    static ChatSystem& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Sending messages
    void sendMessage(const std::string& content);
    void sendTeamMessage(const std::string& content);
    void sendWhisper(const std::string& targetName, const std::string& content);
    void sendSystemMessage(const std::string& content);
    void sendAnnouncement(const std::string& content);
    void sendEmote(const std::string& emoteName);

    // Receiving messages
    void onMessage(MessageCallback callback);
    void receiveMessage(const ChatMessage& message);

    // Message history
    const std::deque<ChatMessage>& getHistory() const { return m_messageHistory; }
    std::vector<ChatMessage> getHistoryByType(ChatMessageType type) const;
    std::vector<ChatMessage> searchHistory(const std::string& query) const;
    void clearHistory();

    // Commands
    void registerCommand(const ChatCommand& command);
    void unregisterCommand(const std::string& name);
    bool executeCommand(const std::string& input);
    std::vector<std::string> getCommandList() const;
    const ChatCommand* getCommand(const std::string& name) const;

    // Built-in commands
    void registerBuiltInCommands();

    // Emotes
    void registerEmote(const EmoteDefinition& emote);
    void unregisterEmote(const std::string& code);
    std::string processEmotes(const std::string& text) const;
    std::vector<std::string> getEmoteList() const;

    // Mute/Block
    void mutePlayer(const std::string& playerName);
    void unmutePlayer(const std::string& playerName);
    void blockPlayer(const std::string& playerName);
    void unblockPlayer(const std::string& playerName);
    bool isPlayerMuted(const std::string& playerName) const;
    bool isPlayerBlocked(const std::string& playerName) const;
    std::vector<std::string> getMutedPlayers() const;
    std::vector<std::string> getBlockedPlayers() const;

    // Profanity filter
    void enableProfanityFilter(bool enabled);
    bool isProfanityFilterEnabled() const { return m_settings.filterProfanity; }
    void addProfanityWord(const std::string& word);
    void removeProfanityWord(const std::string& word);
    std::string filterProfanity(const std::string& text) const;

    // Settings
    void setSettings(const ChatSettings& settings);
    const ChatSettings& getSettings() const { return m_settings; }
    void setMaxMessageLength(int length) { m_settings.maxMessageLength = length; }
    void setMaxHistorySize(int size) { m_settings.maxHistorySize = size; }
    void setRateLimit(float seconds) { m_settings.messageRateLimit = seconds; }

    // User info
    void setLocalPlayerId(uint64_t playerId) { m_localPlayerId = playerId; }
    void setLocalPlayerName(const std::string& name) { m_localPlayerName = name; }
    void setTeam(int team) { m_team = team; }

    // Debug
    int getMessageCount() const { return static_cast<int>(m_messageHistory.size()); }

private:
    ChatSystem();
    ~ChatSystem();
    ChatSystem(const ChatSystem&) = delete;
    ChatSystem& operator=(const ChatSystem&) = delete;

    // Internal message handling
    bool canSendMessage() const;
    bool validateMessage(const std::string& content) const;
    ChatMessage createMessage(const std::string& content, ChatMessageType type);
    void addToHistory(const ChatMessage& message);
    void trimHistory();
    void notifyCallbacks(const ChatMessage& message);

    // Command parsing
    bool isCommand(const std::string& input) const;
    std::pair<std::string, std::vector<std::string>> parseCommand(const std::string& input) const;

    // ID generation
    uint64_t generateMessageId();

private:
    bool m_initialized = false;

    // Local player info
    uint64_t m_localPlayerId = 0;
    std::string m_localPlayerName;
    int m_team = -1;

    // Message history
    std::deque<ChatMessage> m_messageHistory;

    // Commands
    std::unordered_map<std::string, ChatCommand> m_commands;

    // Emotes
    std::unordered_map<std::string, EmoteDefinition> m_emotes;

    // Profanity filter
    std::unordered_set<std::string> m_profanityList;

    // Settings
    ChatSettings m_settings;

    // Rate limiting
    std::chrono::steady_clock::time_point m_lastMessageTime;

    // Callbacks
    std::vector<MessageCallback> m_messageCallbacks;

    // ID generation
    uint64_t m_nextMessageId = 1;
};

} // namespace Replication
} // namespace Network
