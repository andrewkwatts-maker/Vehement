#include "ChatSystem.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace Network {
namespace Replication {

ChatSystem& ChatSystem::getInstance() {
    static ChatSystem instance;
    return instance;
}

ChatSystem::ChatSystem() {
    m_lastMessageTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
}

ChatSystem::~ChatSystem() {
    shutdown();
}

bool ChatSystem::initialize() {
    if (m_initialized) return true;

    registerBuiltInCommands();

    // Register default emotes
    EmoteDefinition smile = {":)", ":)", ""};
    EmoteDefinition sad = {":(", ":(", ""};
    EmoteDefinition laugh = {":D", ":D", ""};
    EmoteDefinition wink = {";)", ";)", ""};

    registerEmote(smile);
    registerEmote(sad);
    registerEmote(laugh);
    registerEmote(wink);

    m_initialized = true;
    return true;
}

void ChatSystem::shutdown() {
    if (!m_initialized) return;

    m_messageHistory.clear();
    m_commands.clear();
    m_emotes.clear();

    m_initialized = false;
}

void ChatSystem::update(float deltaTime) {
    // Nothing to update for now
}

void ChatSystem::sendMessage(const std::string& content) {
    if (content.empty()) return;

    // Check for command
    if (isCommand(content)) {
        executeCommand(content);
        return;
    }

    if (!canSendMessage()) return;
    if (!validateMessage(content)) return;

    ChatMessage message = createMessage(content, ChatMessageType::All);

    addToHistory(message);
    notifyCallbacks(message);

    m_lastMessageTime = std::chrono::steady_clock::now();
}

void ChatSystem::sendTeamMessage(const std::string& content) {
    if (content.empty()) return;
    if (!canSendMessage()) return;
    if (!validateMessage(content)) return;

    ChatMessage message = createMessage(content, ChatMessageType::Team);
    message.team = m_team;

    addToHistory(message);
    notifyCallbacks(message);

    m_lastMessageTime = std::chrono::steady_clock::now();
}

void ChatSystem::sendWhisper(const std::string& targetName, const std::string& content) {
    if (content.empty() || targetName.empty()) return;
    if (!canSendMessage()) return;
    if (!validateMessage(content)) return;

    if (isPlayerBlocked(targetName)) return;

    ChatMessage message = createMessage(content, ChatMessageType::Whisper);
    message.targetName = targetName;

    addToHistory(message);
    notifyCallbacks(message);

    m_lastMessageTime = std::chrono::steady_clock::now();
}

void ChatSystem::sendSystemMessage(const std::string& content) {
    if (content.empty()) return;
    if (!m_settings.showSystemMessages) return;

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = 0;
    message.senderName = "System";
    message.content = content;
    message.type = ChatMessageType::System;
    message.timestamp = std::chrono::system_clock::now();

    addToHistory(message);
    notifyCallbacks(message);
}

void ChatSystem::sendAnnouncement(const std::string& content) {
    if (content.empty()) return;

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = 0;
    message.senderName = "Announcement";
    message.content = content;
    message.type = ChatMessageType::Announcement;
    message.timestamp = std::chrono::system_clock::now();

    addToHistory(message);
    notifyCallbacks(message);
}

void ChatSystem::sendEmote(const std::string& emoteName) {
    ChatMessage message = createMessage(emoteName, ChatMessageType::Emote);
    message.content = "*" + m_localPlayerName + " " + emoteName + "*";

    addToHistory(message);
    notifyCallbacks(message);
}

void ChatSystem::onMessage(MessageCallback callback) {
    m_messageCallbacks.push_back(callback);
}

void ChatSystem::receiveMessage(const ChatMessage& message) {
    // Check if sender is blocked or muted
    if (isPlayerBlocked(message.senderName)) return;
    if (isPlayerMuted(message.senderName)) return;

    // Check team filter for team messages
    if (message.type == ChatMessageType::Team && message.team != m_team) {
        return;
    }

    addToHistory(message);
    notifyCallbacks(message);
}

std::vector<ChatMessage> ChatSystem::getHistoryByType(ChatMessageType type) const {
    std::vector<ChatMessage> result;
    for (const auto& msg : m_messageHistory) {
        if (msg.type == type) {
            result.push_back(msg);
        }
    }
    return result;
}

std::vector<ChatMessage> ChatSystem::searchHistory(const std::string& query) const {
    std::vector<ChatMessage> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& msg : m_messageHistory) {
        std::string lowerContent = msg.content;
        std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);

        if (lowerContent.find(lowerQuery) != std::string::npos) {
            result.push_back(msg);
        }
    }
    return result;
}

void ChatSystem::clearHistory() {
    m_messageHistory.clear();
}

void ChatSystem::registerCommand(const ChatCommand& command) {
    m_commands[command.name] = command;
}

void ChatSystem::unregisterCommand(const std::string& name) {
    m_commands.erase(name);
}

bool ChatSystem::executeCommand(const std::string& input) {
    if (!isCommand(input)) return false;

    auto [cmdName, args] = parseCommand(input);

    auto it = m_commands.find(cmdName);
    if (it == m_commands.end()) {
        sendSystemMessage("Unknown command: " + cmdName);
        return false;
    }

    it->second.handler(m_localPlayerId, args);
    return true;
}

std::vector<std::string> ChatSystem::getCommandList() const {
    std::vector<std::string> result;
    for (const auto& [name, cmd] : m_commands) {
        result.push_back(name);
    }
    return result;
}

const ChatCommand* ChatSystem::getCommand(const std::string& name) const {
    auto it = m_commands.find(name);
    if (it != m_commands.end()) {
        return &it->second;
    }
    return nullptr;
}

void ChatSystem::registerBuiltInCommands() {
    // /help command
    ChatCommand helpCmd;
    helpCmd.name = "help";
    helpCmd.description = "Show available commands";
    helpCmd.usage = "/help [command]";
    helpCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            std::ostringstream ss;
            ss << "Available commands: ";
            for (const auto& [name, cmd] : m_commands) {
                ss << "/" << name << " ";
            }
            sendSystemMessage(ss.str());
        } else {
            auto cmd = getCommand(args[0]);
            if (cmd) {
                sendSystemMessage(cmd->description + " Usage: " + cmd->usage);
            } else {
                sendSystemMessage("Unknown command: " + args[0]);
            }
        }
    };
    registerCommand(helpCmd);

    // /whisper command
    ChatCommand whisperCmd;
    whisperCmd.name = "whisper";
    whisperCmd.description = "Send a private message";
    whisperCmd.usage = "/whisper <player> <message>";
    whisperCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            sendSystemMessage("Usage: /whisper <player> <message>");
            return;
        }
        std::string message;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) message += " ";
            message += args[i];
        }
        sendWhisper(args[0], message);
    };
    registerCommand(whisperCmd);

    // /w alias for whisper
    ChatCommand wCmd = whisperCmd;
    wCmd.name = "w";
    registerCommand(wCmd);

    // /mute command
    ChatCommand muteCmd;
    muteCmd.name = "mute";
    muteCmd.description = "Mute a player";
    muteCmd.usage = "/mute <player>";
    muteCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /mute <player>");
            return;
        }
        mutePlayer(args[0]);
        sendSystemMessage("Muted " + args[0]);
    };
    registerCommand(muteCmd);

    // /unmute command
    ChatCommand unmuteCmd;
    unmuteCmd.name = "unmute";
    unmuteCmd.description = "Unmute a player";
    unmuteCmd.usage = "/unmute <player>";
    unmuteCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /unmute <player>");
            return;
        }
        unmutePlayer(args[0]);
        sendSystemMessage("Unmuted " + args[0]);
    };
    registerCommand(unmuteCmd);

    // /block command
    ChatCommand blockCmd;
    blockCmd.name = "block";
    blockCmd.description = "Block a player";
    blockCmd.usage = "/block <player>";
    blockCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /block <player>");
            return;
        }
        blockPlayer(args[0]);
        sendSystemMessage("Blocked " + args[0]);
    };
    registerCommand(blockCmd);

    // /unblock command
    ChatCommand unblockCmd;
    unblockCmd.name = "unblock";
    unblockCmd.description = "Unblock a player";
    unblockCmd.usage = "/unblock <player>";
    unblockCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /unblock <player>");
            return;
        }
        unblockPlayer(args[0]);
        sendSystemMessage("Unblocked " + args[0]);
    };
    registerCommand(unblockCmd);

    // /clear command
    ChatCommand clearCmd;
    clearCmd.name = "clear";
    clearCmd.description = "Clear chat history";
    clearCmd.usage = "/clear";
    clearCmd.handler = [this](uint64_t, const std::vector<std::string>&) {
        clearHistory();
        sendSystemMessage("Chat cleared");
    };
    registerCommand(clearCmd);

    // /emote command
    ChatCommand emoteCmd;
    emoteCmd.name = "emote";
    emoteCmd.description = "Send an emote";
    emoteCmd.usage = "/emote <action>";
    emoteCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /emote <action>");
            return;
        }
        std::string emote;
        for (const auto& arg : args) {
            if (!emote.empty()) emote += " ";
            emote += arg;
        }
        sendEmote(emote);
    };
    registerCommand(emoteCmd);

    // /me alias for emote
    ChatCommand meCmd = emoteCmd;
    meCmd.name = "me";
    registerCommand(meCmd);

    // /team command
    ChatCommand teamCmd;
    teamCmd.name = "team";
    teamCmd.description = "Send a team message";
    teamCmd.usage = "/team <message>";
    teamCmd.handler = [this](uint64_t, const std::vector<std::string>& args) {
        if (args.empty()) {
            sendSystemMessage("Usage: /team <message>");
            return;
        }
        std::string message;
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) message += " ";
            message += args[i];
        }
        sendTeamMessage(message);
    };
    registerCommand(teamCmd);

    // /t alias for team
    ChatCommand tCmd = teamCmd;
    tCmd.name = "t";
    registerCommand(tCmd);
}

void ChatSystem::registerEmote(const EmoteDefinition& emote) {
    m_emotes[emote.code] = emote;
}

void ChatSystem::unregisterEmote(const std::string& code) {
    m_emotes.erase(code);
}

std::string ChatSystem::processEmotes(const std::string& text) const {
    std::string result = text;

    for (const auto& [code, emote] : m_emotes) {
        size_t pos = 0;
        while ((pos = result.find(code, pos)) != std::string::npos) {
            result.replace(pos, code.length(), emote.displayText);
            pos += emote.displayText.length();
        }
    }

    return result;
}

std::vector<std::string> ChatSystem::getEmoteList() const {
    std::vector<std::string> result;
    for (const auto& [code, emote] : m_emotes) {
        result.push_back(code);
    }
    return result;
}

void ChatSystem::mutePlayer(const std::string& playerName) {
    m_settings.mutedPlayers.insert(playerName);
}

void ChatSystem::unmutePlayer(const std::string& playerName) {
    m_settings.mutedPlayers.erase(playerName);
}

void ChatSystem::blockPlayer(const std::string& playerName) {
    m_settings.blockedPlayers.insert(playerName);
}

void ChatSystem::unblockPlayer(const std::string& playerName) {
    m_settings.blockedPlayers.erase(playerName);
}

bool ChatSystem::isPlayerMuted(const std::string& playerName) const {
    return m_settings.mutedPlayers.find(playerName) != m_settings.mutedPlayers.end();
}

bool ChatSystem::isPlayerBlocked(const std::string& playerName) const {
    return m_settings.blockedPlayers.find(playerName) != m_settings.blockedPlayers.end();
}

std::vector<std::string> ChatSystem::getMutedPlayers() const {
    return std::vector<std::string>(m_settings.mutedPlayers.begin(),
                                     m_settings.mutedPlayers.end());
}

std::vector<std::string> ChatSystem::getBlockedPlayers() const {
    return std::vector<std::string>(m_settings.blockedPlayers.begin(),
                                     m_settings.blockedPlayers.end());
}

void ChatSystem::enableProfanityFilter(bool enabled) {
    m_settings.filterProfanity = enabled;
}

void ChatSystem::addProfanityWord(const std::string& word) {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    m_profanityList.insert(lower);
}

void ChatSystem::removeProfanityWord(const std::string& word) {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    m_profanityList.erase(lower);
}

std::string ChatSystem::filterProfanity(const std::string& text) const {
    if (!m_settings.filterProfanity) return text;

    std::string result = text;
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& word : m_profanityList) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) {
            std::string replacement(word.length(), '*');
            result.replace(pos, word.length(), replacement);
            lower.replace(pos, word.length(), replacement);
            pos += replacement.length();
        }
    }

    return result;
}

void ChatSystem::setSettings(const ChatSettings& settings) {
    m_settings = settings;
}

// Private methods

bool ChatSystem::canSendMessage() const {
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_lastMessageTime).count();
    return elapsed >= m_settings.messageRateLimit;
}

bool ChatSystem::validateMessage(const std::string& content) const {
    if (content.empty()) return false;
    if (static_cast<int>(content.length()) > m_settings.maxMessageLength) return false;
    return true;
}

ChatMessage ChatSystem::createMessage(const std::string& content, ChatMessageType type) {
    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = m_localPlayerId;
    message.senderName = m_localPlayerName;
    message.content = content;
    message.type = type;
    message.timestamp = std::chrono::system_clock::now();

    // Apply profanity filter
    if (m_settings.filterProfanity) {
        message.filteredContent = filterProfanity(content);
        message.isFiltered = message.filteredContent != content;
        if (message.isFiltered) {
            message.content = message.filteredContent;
        }
    }

    // Process emotes
    message.content = processEmotes(message.content);

    return message;
}

void ChatSystem::addToHistory(const ChatMessage& message) {
    m_messageHistory.push_back(message);
    trimHistory();
}

void ChatSystem::trimHistory() {
    while (static_cast<int>(m_messageHistory.size()) > m_settings.maxHistorySize) {
        m_messageHistory.pop_front();
    }
}

void ChatSystem::notifyCallbacks(const ChatMessage& message) {
    for (auto& callback : m_messageCallbacks) {
        callback(message);
    }
}

bool ChatSystem::isCommand(const std::string& input) const {
    return !input.empty() && input[0] == '/';
}

std::pair<std::string, std::vector<std::string>> ChatSystem::parseCommand(
    const std::string& input) const {

    std::string cmdStr = input.substr(1);  // Remove /
    std::vector<std::string> parts;

    std::istringstream iss(cmdStr);
    std::string part;
    while (iss >> part) {
        parts.push_back(part);
    }

    if (parts.empty()) {
        return {"", {}};
    }

    std::string cmdName = parts[0];
    std::vector<std::string> args(parts.begin() + 1, parts.end());

    return {cmdName, args};
}

uint64_t ChatSystem::generateMessageId() {
    return m_nextMessageId++;
}

} // namespace Replication
} // namespace Network
