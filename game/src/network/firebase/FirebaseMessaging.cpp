#include "FirebaseMessaging.hpp"
#include <sstream>
#include <random>
#include <algorithm>
#include <regex>
#include <cctype>

namespace Network {
namespace Firebase {

FirebaseMessaging& FirebaseMessaging::getInstance() {
    static FirebaseMessaging instance;
    return instance;
}

FirebaseMessaging::FirebaseMessaging() {
    // Initialize default profanity list
    m_profanityList = {
        // Add common profanity words here
        // This is a simplified list - production would use a comprehensive list
    };
}

FirebaseMessaging::~FirebaseMessaging() {
    shutdown();
}

bool FirebaseMessaging::initialize() {
    if (m_initialized) return true;

    if (!FirebaseCore::getInstance().isInitialized()) {
        return false;
    }

    m_initialized = true;

    // Load muted/blocked players from storage
    loadMuteBlockList();

    return true;
}

void FirebaseMessaging::shutdown() {
    if (!m_initialized) return;

    // Save mute/block list
    saveMuteBlockList();

    // Process any remaining outgoing messages
    processMessageQueue();

    m_initialized = false;
}

void FirebaseMessaging::update(float deltaTime) {
    if (!m_initialized) return;

    // Poll for new messages
    m_pollTimer += deltaTime;
    if (m_pollTimer >= POLL_INTERVAL) {
        m_pollTimer = 0.0f;
        pollForNewMessages();
    }

    // Process outgoing queue
    processMessageQueue();

    // Update typing indicators
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_typingTimestamps.begin(); it != m_typingTimestamps.end();) {
        float elapsed = std::chrono::duration<float>(now - it->second).count();
        if (elapsed >= TYPING_TIMEOUT) {
            if (m_typingCallback) {
                m_typingCallback(it->first, "", false);
            }
            it = m_typingTimestamps.erase(it);
        } else {
            ++it;
        }
    }
}

void FirebaseMessaging::sendMessage(const std::string& recipientId, const std::string& content,
    std::function<void(const ChatMessage&, const FirebaseError&)> callback) {

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = FirebaseCore::getInstance().getCurrentUser().uid;
    message.senderName = FirebaseCore::getInstance().getCurrentUser().displayName;
    message.recipientId = recipientId;
    message.type = MessageType::PlayerToPlayer;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();

    sendMessage(message, callback);
}

void FirebaseMessaging::sendMessage(const ChatMessage& message,
    std::function<void(const ChatMessage&, const FirebaseError&)> callback) {

    // Rate limiting
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_lastMessageTime).count();
    if (elapsed < MIN_MESSAGE_INTERVAL) {
        FirebaseError error;
        error.type = FirebaseErrorType::RateLimited;
        error.message = "Sending messages too quickly";
        if (callback) callback(ChatMessage(), error);
        return;
    }
    m_lastMessageTime = now;

    // Apply filters
    ChatMessage filteredMessage = applyFilters(message);

    // Check if recipient is valid
    if (filteredMessage.type == MessageType::PlayerToPlayer) {
        if (isPlayerBlocked(filteredMessage.recipientId)) {
            FirebaseError error;
            error.type = FirebaseErrorType::PermissionDenied;
            error.message = "Cannot send message to blocked player";
            if (callback) callback(ChatMessage(), error);
            return;
        }
    }

    uploadMessage(filteredMessage, callback);
}

void FirebaseMessaging::sendToChannel(const std::string& channelId, const std::string& content,
    std::function<void(const ChatMessage&, const FirebaseError&)> callback) {

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = FirebaseCore::getInstance().getCurrentUser().uid;
    message.senderName = FirebaseCore::getInstance().getCurrentUser().displayName;
    message.channelId = channelId;
    message.type = MessageType::GlobalChat;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();

    sendMessage(message, callback);
}

void FirebaseMessaging::sendToTeam(const std::string& content) {
    if (m_currentTeamId.empty()) return;

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = FirebaseCore::getInstance().getCurrentUser().uid;
    message.senderName = FirebaseCore::getInstance().getCurrentUser().displayName;
    message.channelId = "team_" + m_currentTeamId;
    message.type = MessageType::TeamChat;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();

    sendMessage(message, nullptr);
}

void FirebaseMessaging::sendToMatch(const std::string& content) {
    if (m_currentMatchId.empty()) return;

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = FirebaseCore::getInstance().getCurrentUser().uid;
    message.senderName = FirebaseCore::getInstance().getCurrentUser().displayName;
    message.channelId = "match_" + m_currentMatchId;
    message.type = MessageType::MatchChat;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();

    sendMessage(message, nullptr);
}

void FirebaseMessaging::sendToLobby(const std::string& content) {
    if (m_currentLobbyId.empty()) return;

    ChatMessage message;
    message.messageId = generateMessageId();
    message.senderId = FirebaseCore::getInstance().getCurrentUser().uid;
    message.senderName = FirebaseCore::getInstance().getCurrentUser().displayName;
    message.channelId = "lobby_" + m_currentLobbyId;
    message.type = MessageType::LobbyChat;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();

    sendMessage(message, nullptr);
}

void FirebaseMessaging::editMessage(const std::string& messageId, const std::string& newContent,
    std::function<void(const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/messages/" + messageId;
    request.headers["Content-Type"] = "application/json";

    FilterResult filtered = filterMessage(newContent);

    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"content\":{\"stringValue\":\"" << newContent << "\"},"
         << "\"filteredContent\":{\"stringValue\":\"" << filtered.filtered << "\"},"
         << "\"isFiltered\":{\"booleanValue\":" << (filtered.wasFiltered ? "true" : "false") << "},"
         << "\"isEdited\":{\"booleanValue\":true},"
         << "\"editedAt\":{\"timestampValue\":\"" << getCurrentTimestamp() << "\"}"
         << "}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            if (callback) callback(FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(error);
        }
    });
}

void FirebaseMessaging::deleteMessage(const std::string& messageId,
    std::function<void(const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    // Soft delete - mark as deleted rather than removing
    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/messages/" + messageId;
    request.headers["Content-Type"] = "application/json";
    request.body = "{\"fields\":{\"isDeleted\":{\"booleanValue\":true}}}";

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            if (callback) callback(FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(error);
        }
    });
}

void FirebaseMessaging::addReaction(const std::string& messageId, const std::string& emoji) {
    auto& core = FirebaseCore::getInstance();
    std::string oderId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/messages/" + messageId;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"fields\":{\"reactions." << emoji << "\":{\"arrayValue\":{\"values\":[{\"stringValue\":\"" << oderId << "\"}]}}}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
}

void FirebaseMessaging::removeReaction(const std::string& messageId, const std::string& emoji) {
    // Would need to fetch current reactions and remove user from array
    // Simplified - in production use array remove operation
}

void FirebaseMessaging::getMessageHistory(const std::string& channelOrConversationId, int count,
    int offset, MessageListCallback callback) {

    downloadMessages(channelOrConversationId, count, offset, callback);
}

void FirebaseMessaging::getMessagesBefore(const std::string& channelId,
    const std::string& beforeMessageId, int count, MessageListCallback callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"messages\"}],"
          << "\"where\":{"
          << "\"compositeFilter\":{"
          << "\"op\":\"AND\","
          << "\"filters\":["
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"channelId\"},\"op\":\"EQUAL\",\"value\":{\"stringValue\":\"" << channelId << "\"}}},"
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"messageId\"},\"op\":\"LESS_THAN\",\"value\":{\"stringValue\":\"" << beforeMessageId << "\"}}}"
          << "]"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"timestamp\"},\"direction\":\"DESCENDING\"}],"
          << "\"limit\":" << count
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<ChatMessage> messages = parseMessageList(response.body);
            callback(messages, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::getMessagesAfter(const std::string& channelId,
    const std::string& afterMessageId, int count, MessageListCallback callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"messages\"}],"
          << "\"where\":{"
          << "\"compositeFilter\":{"
          << "\"op\":\"AND\","
          << "\"filters\":["
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"channelId\"},\"op\":\"EQUAL\",\"value\":{\"stringValue\":\"" << channelId << "\"}}},"
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"messageId\"},\"op\":\"GREATER_THAN\",\"value\":{\"stringValue\":\"" << afterMessageId << "\"}}}"
          << "]"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"timestamp\"},\"direction\":\"ASCENDING\"}],"
          << "\"limit\":" << count
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<ChatMessage> messages = parseMessageList(response.body);
            callback(messages, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::searchMessages(const std::string& query, int count,
    MessageListCallback callback) {

    // Firestore doesn't support full-text search natively
    // Would need to use Algolia, Elasticsearch, or Cloud Functions
    // Simplified client-side search
    callback({}, FirebaseError());
}

void FirebaseMessaging::getConversations(
    std::function<void(const std::vector<Conversation>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"conversations\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"participantIds\"},"
          << "\"op\":\"ARRAY_CONTAINS\","
          << "\"value\":{\"stringValue\":\"" << myId << "\"}"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"lastMessageAt\"},\"direction\":\"DESCENDING\"}]"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<Conversation> conversations = parseConversationList(response.body);

            // Cache conversations
            for (const auto& conv : conversations) {
                m_conversations[conv.conversationId] = conv;
            }

            callback(conversations, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::getConversation(const std::string& oderId,
    ConversationCallback callback) {

    std::string conversationId = getConversationId(
        FirebaseCore::getInstance().getCurrentUser().uid, oderId);

    auto it = m_conversations.find(conversationId);
    if (it != m_conversations.end()) {
        callback(it->second, FirebaseError());
        return;
    }

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() + "/conversations/" + conversationId;

    core.makeAuthenticatedRequest(request, [this, conversationId, callback](
        const HttpResponse& response) {

        if (response.statusCode == 200) {
            Conversation conv = deserializeConversation(response.body);
            m_conversations[conversationId] = conv;
            callback(conv, FirebaseError());
        } else if (response.statusCode == 404) {
            // Create new conversation
            Conversation newConv;
            newConv.conversationId = conversationId;
            callback(newConv, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback(Conversation(), error);
        }
    });
}

void FirebaseMessaging::markAsRead(const std::string& conversationId) {
    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/conversations/" + conversationId;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"fields\":{\"unreadCounts." << myId << "\":{\"integerValue\":0}}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [this, conversationId](const HttpResponse&) {
        // Update local count
        m_unreadCounts[conversationId] = 0;
        recalculateTotalUnread();
    });
}

int FirebaseMessaging::getUnreadCount() const {
    return m_totalUnreadCount;
}

int FirebaseMessaging::getUnreadCount(const std::string& conversationOrChannelId) const {
    auto it = m_unreadCounts.find(conversationOrChannelId);
    return it != m_unreadCounts.end() ? it->second : 0;
}

void FirebaseMessaging::createChannel(const std::string& name, MessageType type,
    bool isPublic, ChannelCallback callback) {

    auto& core = FirebaseCore::getInstance();

    ChatChannel channel;
    channel.channelId = generateChannelId();
    channel.name = name;
    channel.type = type;
    channel.isPublic = isPublic;
    channel.ownerId = core.getCurrentUser().uid;
    channel.memberIds.push_back(channel.ownerId);
    channel.moderatorIds.push_back(channel.ownerId);
    channel.createdAt = std::chrono::system_clock::now();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + "/channels?documentId=" + channel.channelId;
    request.headers["Content-Type"] = "application/json";
    request.body = serializeChannel(channel);

    core.makeAuthenticatedRequest(request, [this, channel, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 201) {
            m_channelCache[channel.channelId] = channel;
            m_joinedChannels.push_back(channel.channelId);
            callback(channel, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback(ChatChannel(), error);
        }
    });
}

void FirebaseMessaging::deleteChannel(const std::string& channelId,
    std::function<void(const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    // Check if owner
    auto it = m_channelCache.find(channelId);
    if (it != m_channelCache.end() && it->second.ownerId != core.getCurrentUser().uid) {
        FirebaseError error;
        error.type = FirebaseErrorType::PermissionDenied;
        error.message = "Only channel owner can delete";
        if (callback) callback(error);
        return;
    }

    HttpRequest request;
    request.method = "DELETE";
    request.url = core.getConfig().getFirestoreUrl() + "/channels/" + channelId;

    core.makeAuthenticatedRequest(request, [this, channelId, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 204) {
            m_channelCache.erase(channelId);
            m_joinedChannels.erase(
                std::remove(m_joinedChannels.begin(), m_joinedChannels.end(), channelId),
                m_joinedChannels.end());
            if (callback) callback(FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            if (callback) callback(error);
        }
    });
}

void FirebaseMessaging::joinChannel(const std::string& channelId,
    std::function<void(const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/channels/" + channelId;
    request.headers["Content-Type"] = "application/json";

    // Add user to memberIds array
    std::ostringstream json;
    json << "{\"fields\":{\"memberIds\":{\"arrayValue\":{\"values\":[{\"stringValue\":\"" << myId << "\"}]}}}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [this, channelId, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            m_joinedChannels.push_back(channelId);
            if (callback) callback(FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            if (callback) callback(error);
        }
    });
}

void FirebaseMessaging::leaveChannel(const std::string& channelId) {
    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    // Remove from joined channels
    m_joinedChannels.erase(
        std::remove(m_joinedChannels.begin(), m_joinedChannels.end(), channelId),
        m_joinedChannels.end());

    // Update server
    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/channels/" + channelId;
    request.headers["Content-Type"] = "application/json";
    // Would need to use array remove operation
    request.body = "{}";

    core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
}

void FirebaseMessaging::getChannel(const std::string& channelId, ChannelCallback callback) {
    auto it = m_channelCache.find(channelId);
    if (it != m_channelCache.end()) {
        callback(it->second, FirebaseError());
        return;
    }

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() + "/channels/" + channelId;

    core.makeAuthenticatedRequest(request, [this, channelId, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            ChatChannel channel = deserializeChannel(response.body);
            m_channelCache[channelId] = channel;
            callback(channel, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback(ChatChannel(), error);
        }
    });
}

void FirebaseMessaging::listChannels(MessageType type,
    std::function<void(const std::vector<ChatChannel>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"channels\"}],"
          << "\"where\":{"
          << "\"compositeFilter\":{"
          << "\"op\":\"AND\","
          << "\"filters\":["
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"type\"},\"op\":\"EQUAL\",\"value\":{\"integerValue\":" << static_cast<int>(type) << "}}},"
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"isPublic\"},\"op\":\"EQUAL\",\"value\":{\"booleanValue\":true}}}"
          << "]"
          << "}"
          << "}"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<ChatChannel> channels = parseChannelList(response.body);
            callback(channels, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::listMyChannels(
    std::function<void(const std::vector<ChatChannel>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"channels\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"memberIds\"},"
          << "\"op\":\"ARRAY_CONTAINS\","
          << "\"value\":{\"stringValue\":\"" << myId << "\"}"
          << "}"
          << "}"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<ChatChannel> channels = parseChannelList(response.body);
            callback(channels, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::addModerator(const std::string& channelId, const std::string& oderId) {
    // Similar to joinChannel but adds to moderatorIds
}

void FirebaseMessaging::removeModerator(const std::string& channelId, const std::string& oderId) {
    // Remove from moderatorIds array
}

void FirebaseMessaging::kickFromChannel(const std::string& channelId, const std::string& oderId) {
    // Remove from memberIds array
}

void FirebaseMessaging::banFromChannel(const std::string& channelId, const std::string& oderId,
    std::chrono::seconds duration) {
    // Add to bannedIds with expiration
}

void FirebaseMessaging::registerForPushNotifications(const std::string& deviceToken) {
    m_deviceToken = deviceToken;

    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/players/" + myId;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"fields\":{\"pushToken\":{\"stringValue\":\"" << deviceToken << "\"}}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
}

void FirebaseMessaging::unregisterFromPushNotifications() {
    m_deviceToken.clear();

    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/players/" + myId;
    request.headers["Content-Type"] = "application/json";
    request.body = "{\"fields\":{\"pushToken\":{\"nullValue\":null}}}";

    core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
}

void FirebaseMessaging::sendPushNotification(const std::string& recipientId,
    const PushNotification& notification) {

    // Would use Firebase Cloud Messaging (FCM) API
    // REST API: https://fcm.googleapis.com/v1/projects/{project}/messages:send
}

void FirebaseMessaging::onPushNotification(NotificationCallback callback) {
    m_notificationCallbacks.push_back(callback);
}

void FirebaseMessaging::mutePlayer(const std::string& oderId, std::chrono::seconds duration) {
    MuteEntry entry;
    entry.oderId = oderId;
    entry.mutedAt = std::chrono::system_clock::now();
    entry.isPermanent = duration.count() == 0;

    if (!entry.isPermanent) {
        entry.expiresAt = entry.mutedAt + duration;
    }

    m_mutedPlayers[oderId] = entry;
    saveMuteBlockList();
}

void FirebaseMessaging::unmutePlayer(const std::string& oderId) {
    m_mutedPlayers.erase(oderId);
    saveMuteBlockList();
}

void FirebaseMessaging::blockPlayer(const std::string& oderId) {
    m_blockedPlayers.insert(oderId);
    saveMuteBlockList();
}

void FirebaseMessaging::unblockPlayer(const std::string& oderId) {
    m_blockedPlayers.erase(oderId);
    saveMuteBlockList();
}

bool FirebaseMessaging::isPlayerMuted(const std::string& oderId) const {
    auto it = m_mutedPlayers.find(oderId);
    if (it == m_mutedPlayers.end()) return false;

    // Check if expired
    if (!it->second.isPermanent) {
        if (std::chrono::system_clock::now() >= it->second.expiresAt) {
            return false;
        }
    }

    return true;
}

bool FirebaseMessaging::isPlayerBlocked(const std::string& oderId) const {
    return m_blockedPlayers.find(oderId) != m_blockedPlayers.end();
}

std::vector<MuteEntry> FirebaseMessaging::getMutedPlayers() const {
    std::vector<MuteEntry> result;
    for (const auto& [id, entry] : m_mutedPlayers) {
        result.push_back(entry);
    }
    return result;
}

std::vector<std::string> FirebaseMessaging::getBlockedPlayers() const {
    return std::vector<std::string>(m_blockedPlayers.begin(), m_blockedPlayers.end());
}

FilterResult FirebaseMessaging::filterMessage(const std::string& content) {
    FilterResult result;
    result.original = content;
    result.filtered = content;

    if (!m_profanityFilterEnabled) {
        return result;
    }

    std::string lower = content;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check against profanity list
    for (const auto& word : m_profanityList) {
        size_t pos = lower.find(word);
        while (pos != std::string::npos) {
            result.wasFiltered = true;
            result.flaggedWords.push_back(word);

            // Replace with asterisks
            std::string replacement(word.length(), '*');
            result.filtered.replace(pos, word.length(), replacement);

            pos = lower.find(word, pos + 1);
        }
    }

    // Check custom filters
    for (const auto& word : m_customFilters) {
        size_t pos = lower.find(word);
        while (pos != std::string::npos) {
            result.wasFiltered = true;
            result.flaggedWords.push_back(word);

            std::string replacement(word.length(), '*');
            result.filtered.replace(pos, word.length(), replacement);

            pos = lower.find(word, pos + 1);
        }
    }

    // Simple toxicity score based on flagged words
    result.toxicityScore = std::min(1.0f,
        static_cast<float>(result.flaggedWords.size()) * 0.2f);

    return result;
}

void FirebaseMessaging::addCustomFilter(const std::string& word) {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    m_customFilters.insert(lower);
}

void FirebaseMessaging::removeCustomFilter(const std::string& word) {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    m_customFilters.erase(lower);
}

void FirebaseMessaging::onMessage(MessageCallback callback) {
    m_globalMessageCallbacks.push_back(callback);
}

void FirebaseMessaging::onMessageInChannel(const std::string& channelId, MessageCallback callback) {
    m_channelMessageCallbacks[channelId].push_back(callback);
}

void FirebaseMessaging::removeMessageListener(const std::string& channelId) {
    if (channelId.empty()) {
        m_globalMessageCallbacks.clear();
    } else {
        m_channelMessageCallbacks.erase(channelId);
    }
}

void FirebaseMessaging::setTyping(const std::string& channelOrConversationId, bool isTyping) {
    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    std::string path = "typing/" + channelOrConversationId + "/" + myId;

    if (isTyping) {
        HttpRequest request;
        request.method = "PUT";
        request.url = core.getConfig().getRealtimeDbUrl() + "/" + path + ".json";
        request.headers["Content-Type"] = "application/json";
        request.body = "{\"timestamp\":{\".sv\":\"timestamp\"}}";

        core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
    } else {
        HttpRequest request;
        request.method = "DELETE";
        request.url = core.getConfig().getRealtimeDbUrl() + "/" + path + ".json";

        core.makeAuthenticatedRequest(request, [](const HttpResponse&) {});
    }
}

void FirebaseMessaging::onTypingIndicator(
    std::function<void(const std::string&, const std::string&, bool)> callback) {
    m_typingCallback = callback;
}

// Private methods

void FirebaseMessaging::uploadMessage(const ChatMessage& message,
    std::function<void(const ChatMessage&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + "/messages?documentId=" + message.messageId;
    request.headers["Content-Type"] = "application/json";
    request.body = serializeMessage(message);

    core.makeAuthenticatedRequest(request, [message, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 201) {
            if (callback) callback(message, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            error.type = FirebaseErrorType::ServerError;
            if (callback) callback(ChatMessage(), error);
        }
    });
}

void FirebaseMessaging::downloadMessages(const std::string& path, int count, int offset,
    MessageListCallback callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"messages\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"channelId\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"stringValue\":\"" << path << "\"}"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"timestamp\"},\"direction\":\"DESCENDING\"}],"
          << "\"offset\":" << offset << ","
          << "\"limit\":" << count
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::vector<ChatMessage> messages = parseMessageList(response.body);
            callback(messages, FirebaseError());
        } else {
            FirebaseError error;
            error.code = response.statusCode;
            callback({}, error);
        }
    });
}

void FirebaseMessaging::handleIncomingMessage(const ChatMessage& message) {
    // Check if sender is muted or blocked
    if (isPlayerMuted(message.senderId) || isPlayerBlocked(message.senderId)) {
        return;
    }

    // Apply filters
    ChatMessage filtered = applyFilters(message);

    // Update unread count
    std::string key = filtered.channelId.empty() ?
        getConversationId(filtered.senderId, FirebaseCore::getInstance().getCurrentUser().uid) :
        filtered.channelId;

    m_unreadCounts[key]++;
    recalculateTotalUnread();

    // Cache message
    m_messageCache[key].push_back(filtered);

    // Notify listeners
    for (auto& callback : m_globalMessageCallbacks) {
        callback(filtered);
    }

    auto it = m_channelMessageCallbacks.find(filtered.channelId);
    if (it != m_channelMessageCallbacks.end()) {
        for (auto& callback : it->second) {
            callback(filtered);
        }
    }

    // Create local notification if app is backgrounded
    createLocalNotification(filtered);
}

void FirebaseMessaging::createLocalNotification(const ChatMessage& message) {
    if (!m_notificationsEnabled) return;

    PushNotification notification;
    notification.title = message.senderName;
    notification.body = message.isFiltered ? message.filteredContent : message.content;
    notification.data["messageId"] = message.messageId;
    notification.data["senderId"] = message.senderId;

    for (auto& callback : m_notificationCallbacks) {
        callback(notification);
    }
}

bool FirebaseMessaging::shouldFilterMessage(const ChatMessage& message) const {
    return m_profanityFilterEnabled;
}

ChatMessage FirebaseMessaging::applyFilters(const ChatMessage& message) {
    ChatMessage filtered = message;

    if (shouldFilterMessage(message)) {
        FilterResult result = filterMessage(message.content);
        filtered.filteredContent = result.filtered;
        filtered.isFiltered = result.wasFiltered;

        if (result.toxicityScore > 0.5f) {
            filtered.modAction = ModerationAction::Flagged;
        }
    }

    return filtered;
}

void FirebaseMessaging::pollForNewMessages() {
    // Poll joined channels for new messages
    for (const auto& channelId : m_joinedChannels) {
        // Get messages newer than last known
        // This is simplified - production would use proper cursors
    }
}

void FirebaseMessaging::processMessageQueue() {
    while (!m_outgoingQueue.empty()) {
        ChatMessage message = m_outgoingQueue.front();
        m_outgoingQueue.pop();

        uploadMessage(message, nullptr);
    }
}

std::string FirebaseMessaging::serializeMessage(const ChatMessage& message) {
    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"messageId\":{\"stringValue\":\"" << message.messageId << "\"},"
         << "\"senderId\":{\"stringValue\":\"" << message.senderId << "\"},"
         << "\"senderName\":{\"stringValue\":\"" << message.senderName << "\"},"
         << "\"content\":{\"stringValue\":\"" << escapeJson(message.content) << "\"},"
         << "\"type\":{\"integerValue\":" << static_cast<int>(message.type) << "},"
         << "\"timestamp\":{\"timestampValue\":\"" << getCurrentTimestamp() << "\"}";

    if (!message.recipientId.empty()) {
        json << ",\"recipientId\":{\"stringValue\":\"" << message.recipientId << "\"}";
    }
    if (!message.channelId.empty()) {
        json << ",\"channelId\":{\"stringValue\":\"" << message.channelId << "\"}";
    }
    if (message.isFiltered) {
        json << ",\"filteredContent\":{\"stringValue\":\"" << escapeJson(message.filteredContent) << "\"}";
        json << ",\"isFiltered\":{\"booleanValue\":true}";
    }

    json << "}}";
    return json.str();
}

ChatMessage FirebaseMessaging::deserializeMessage(const std::string& json) {
    ChatMessage message;
    // Parse JSON - simplified
    return message;
}

std::string FirebaseMessaging::serializeChannel(const ChatChannel& channel) {
    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"channelId\":{\"stringValue\":\"" << channel.channelId << "\"},"
         << "\"name\":{\"stringValue\":\"" << channel.name << "\"},"
         << "\"type\":{\"integerValue\":" << static_cast<int>(channel.type) << "},"
         << "\"ownerId\":{\"stringValue\":\"" << channel.ownerId << "\"},"
         << "\"isPublic\":{\"booleanValue\":" << (channel.isPublic ? "true" : "false") << "},"
         << "\"memberIds\":{\"arrayValue\":{\"values\":[";

    for (size_t i = 0; i < channel.memberIds.size(); i++) {
        if (i > 0) json << ",";
        json << "{\"stringValue\":\"" << channel.memberIds[i] << "\"}";
    }

    json << "]}}}}";
    return json.str();
}

ChatChannel FirebaseMessaging::deserializeChannel(const std::string& json) {
    ChatChannel channel;
    // Parse JSON - simplified
    return channel;
}

std::string FirebaseMessaging::generateMessageId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "msg_";
    for (int i = 0; i < 20; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string FirebaseMessaging::generateChannelId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "ch_";
    for (int i = 0; i < 16; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string FirebaseMessaging::getConversationId(const std::string& oderId1,
    const std::string& oderId2) {
    // Ensure consistent ordering
    if (oderId1 < oderId2) {
        return "conv_" + oderId1 + "_" + oderId2;
    }
    return "conv_" + oderId2 + "_" + oderId1;
}

// Helper functions
namespace {

void loadMuteBlockList() {
    // Load from local storage
}

void saveMuteBlockList() {
    // Save to local storage
}

void recalculateTotalUnread() {
    // Sum up all unread counts
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::vector<ChatMessage> parseMessageList(const std::string& json) {
    std::vector<ChatMessage> messages;
    // Parse JSON array
    return messages;
}

std::vector<Conversation> parseConversationList(const std::string& json) {
    std::vector<Conversation> conversations;
    // Parse JSON array
    return conversations;
}

Conversation deserializeConversation(const std::string& json) {
    Conversation conv;
    // Parse JSON
    return conv;
}

std::vector<ChatChannel> parseChannelList(const std::string& json) {
    std::vector<ChatChannel> channels;
    // Parse JSON array
    return channels;
}

} // anonymous namespace

} // namespace Firebase
} // namespace Network
