#include "FirebaseCore.hpp"
#include <sstream>
#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>

// JSON parsing helpers (simple implementation)
// In production, use nlohmann/json or similar
namespace {

std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t startQuote = json.find('"', colonPos);
    if (startQuote == std::string::npos) return "";

    size_t endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return "";

    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

int extractJsonInt(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return 0;

    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    size_t end = start;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) end++;

    try {
        return std::stoi(json.substr(start, end - start));
    } catch (...) {
        return 0;
    }
}

bool extractJsonBool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return false;

    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    return json.substr(start, 4) == "true";
}

std::string urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }
    return uuid;
}

} // anonymous namespace

namespace Network {
namespace Firebase {

FirebaseCore& FirebaseCore::getInstance() {
    static FirebaseCore instance;
    return instance;
}

FirebaseCore::FirebaseCore() {
    m_cacheFilePath = "firebase_cache.dat";
    m_pendingOpsFilePath = "firebase_pending.dat";
}

FirebaseCore::~FirebaseCore() {
    shutdown();
}

bool FirebaseCore::initialize(const FirebaseConfig& config) {
    if (m_initialized) {
        return true;
    }

    if (config.apiKey.empty() || config.projectId.empty()) {
        m_lastError.type = FirebaseErrorType::InvalidArgument;
        m_lastError.message = "Invalid Firebase configuration";
        return false;
    }

    m_config = config;
    m_initialized = true;

    // Load cached data
    if (m_persistenceEnabled) {
        loadCacheFromDisk();
        loadPendingOperations();
    }

    updateConnectionState(ConnectionState::Disconnected);

    return true;
}

void FirebaseCore::shutdown() {
    if (!m_initialized) return;

    // Save any pending data
    if (m_persistenceEnabled) {
        saveCacheToDisk();
        savePendingOperations();
    }

    signOut();
    m_initialized = false;
    updateConnectionState(ConnectionState::Disconnected);
}

void FirebaseCore::signInAnonymously(AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signUp?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";
    request.body = "{\"returnSecureToken\":true}";

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signInWithEmail(const std::string& email, const std::string& password, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signInWithPassword?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"email\":\"" << email << "\",\"password\":\"" << password << "\",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signUpWithEmail(const std::string& email, const std::string& password, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signUp?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"email\":\"" << email << "\",\"password\":\"" << password << "\",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signInWithGoogle(const std::string& idToken, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signInWithIdp?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"postBody\":\"id_token=" << idToken << "&providerId=google.com\","
         << "\"requestUri\":\"http://localhost\","
         << "\"returnSecureToken\":true,"
         << "\"returnIdpCredential\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signInWithApple(const std::string& idToken, const std::string& nonce, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signInWithIdp?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"postBody\":\"id_token=" << idToken << "&nonce=" << nonce << "&providerId=apple.com\","
         << "\"requestUri\":\"http://localhost\","
         << "\"returnSecureToken\":true,"
         << "\"returnIdpCredential\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signInWithCustomToken(const std::string& customToken, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:signInWithCustomToken?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"token\":\"" << customToken << "\",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::signOut() {
    std::lock_guard<std::mutex> lock(m_authMutex);

    m_isSignedIn = false;
    m_currentUser = FirebaseUser();
    m_authToken = AuthToken();

    // Notify listeners
    std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
    FirebaseError noError;
    for (auto& callback : m_authCallbacks) {
        callback(m_currentUser, noError);
    }
}

void FirebaseCore::deleteAccount(AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();
    FIREBASE_CHECK_AUTH();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:delete?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"idToken\":\"" << m_authToken.idToken << "\"}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            signOut();
            if (callback) {
                callback(FirebaseUser(), FirebaseError());
            }
        } else {
            if (callback) {
                callback(FirebaseUser(), parseError(response));
            }
        }
    });
}

void FirebaseCore::sendPasswordReset(const std::string& email, std::function<void(const FirebaseError&)> callback) {
    FIREBASE_CHECK_INITIALIZED();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:sendOobCode?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"requestType\":\"PASSWORD_RESET\",\"email\":\"" << email << "\"}";
    request.body = body.str();

    makeRequest(request, [callback](const HttpResponse& response) {
        if (callback) {
            if (response.statusCode == 200) {
                callback(FirebaseError());
            } else {
                FirebaseError err;
                err.type = FirebaseErrorType::ServerError;
                err.message = "Failed to send password reset";
                callback(err);
            }
        }
    });
}

void FirebaseCore::updatePassword(const std::string& newPassword, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();
    FIREBASE_CHECK_AUTH();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:update?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"idToken\":\"" << m_authToken.idToken << "\",\"password\":\"" << newPassword << "\",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::updateEmail(const std::string& newEmail, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();
    FIREBASE_CHECK_AUTH();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:update?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"idToken\":\"" << m_authToken.idToken << "\",\"email\":\"" << newEmail << "\",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::updateProfile(const std::string& displayName, const std::string& photoUrl, AuthCallback callback) {
    FIREBASE_CHECK_INITIALIZED();
    FIREBASE_CHECK_AUTH();

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getAuthUrl() + "/accounts:update?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/json";

    std::ostringstream body;
    body << "{\"idToken\":\"" << m_authToken.idToken << "\"";
    if (!displayName.empty()) {
        body << ",\"displayName\":\"" << displayName << "\"";
    }
    if (!photoUrl.empty()) {
        body << ",\"photoUrl\":\"" << photoUrl << "\"";
    }
    body << ",\"returnSecureToken\":true}";
    request.body = body.str();

    makeRequest(request, [this, callback](const HttpResponse& response) {
        handleAuthResponse(response, callback);
    });
}

void FirebaseCore::refreshToken(TokenCallback callback) {
    if (!m_initialized || m_authToken.refreshToken.empty()) {
        if (callback) {
            FirebaseError err;
            err.type = FirebaseErrorType::AuthError;
            err.message = "No refresh token available";
            callback("", err);
        }
        return;
    }

    HttpRequest request;
    request.method = "POST";
    request.url = m_config.getTokenRefreshUrl() + "?key=" + m_config.apiKey;
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.body = "grant_type=refresh_token&refresh_token=" + m_authToken.refreshToken;

    makeRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            std::lock_guard<std::mutex> lock(m_authMutex);

            m_authToken.idToken = extractJsonString(response.body, "id_token");
            m_authToken.refreshToken = extractJsonString(response.body, "refresh_token");

            int expiresIn = extractJsonInt(response.body, "expires_in");
            m_authToken.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);

            if (callback) {
                callback(m_authToken.idToken, FirebaseError());
            }
        } else {
            if (callback) {
                callback("", parseError(response));
            }
        }
    });
}

std::string FirebaseCore::getIdToken() const {
    std::lock_guard<std::mutex> lock(m_authMutex);
    return m_authToken.idToken;
}

bool FirebaseCore::isTokenValid() const {
    std::lock_guard<std::mutex> lock(m_authMutex);
    return !m_authToken.idToken.empty() && !m_authToken.isExpired();
}

void FirebaseCore::onAuthStateChanged(AuthCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_authCallbacks.push_back(callback);
}

void FirebaseCore::onConnectionStateChanged(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_connectionCallbacks.push_back(callback);
}

void FirebaseCore::reconnect() {
    if (m_connectionState == ConnectionState::Connected) return;

    updateConnectionState(ConnectionState::Reconnecting);
    m_reconnectAttempts++;

    // Test connection with a simple request
    HttpRequest request;
    request.method = "GET";
    request.url = m_config.getRealtimeDbUrl() + "/.info/connected.json";

    makeRequest(request, [this](const HttpResponse& response) {
        if (response.statusCode == 200) {
            m_reconnectAttempts = 0;
            updateConnectionState(ConnectionState::Connected);

            // Process any pending operations
            if (m_persistenceEnabled) {
                processPendingOperations();
            }
        } else {
            updateConnectionState(ConnectionState::Error);
        }
    });
}

void FirebaseCore::goOffline() {
    m_isOnline = false;
    updateConnectionState(ConnectionState::Disconnected);
}

void FirebaseCore::goOnline() {
    m_isOnline = true;
    reconnect();
}

void FirebaseCore::makeRequest(const HttpRequest& request, HttpCallback callback) {
    executeHttpRequest(request, callback);
}

void FirebaseCore::makeAuthenticatedRequest(const HttpRequest& request, HttpCallback callback) {
    // Check if token needs refresh
    if (m_authToken.needsRefresh() && !m_authToken.refreshToken.empty()) {
        refreshToken([this, request, callback](const std::string& token, const FirebaseError& error) {
            if (error) {
                HttpResponse response;
                response.error = error;
                callback(response);
                return;
            }

            HttpRequest authRequest = request;
            authRequest.headers["Authorization"] = "Bearer " + token;
            executeHttpRequest(authRequest, callback);
        });
    } else {
        HttpRequest authRequest = request;
        if (!m_authToken.idToken.empty()) {
            authRequest.headers["Authorization"] = "Bearer " + m_authToken.idToken;
        }
        executeHttpRequest(authRequest, callback);
    }
}

void FirebaseCore::enablePersistence(bool enabled) {
    m_persistenceEnabled = enabled;

    if (enabled) {
        loadCacheFromDisk();
        loadPendingOperations();
    }
}

void FirebaseCore::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();

    // Delete cache file
    std::remove(m_cacheFilePath.c_str());
}

void FirebaseCore::setCache(const std::string& key, const std::string& data, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    CacheEntry entry;
    entry.key = key;
    entry.data = data;
    entry.timestamp = std::chrono::system_clock::now();
    entry.expiresAt = entry.timestamp + ttl;

    m_cache[key] = entry;
}

std::string FirebaseCore::getCache(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(key);
    if (it == m_cache.end()) return "";

    // Check expiration
    if (std::chrono::system_clock::now() >= it->second.expiresAt) {
        m_cache.erase(it);
        return "";
    }

    return it->second.data;
}

bool FirebaseCore::hasCache(const std::string& key) {
    return !getCache(key).empty();
}

void FirebaseCore::addPendingOperation(const PendingOperation& op) {
    std::lock_guard<std::mutex> lock(m_pendingOpsMutex);

    PendingOperation newOp = op;
    if (newOp.id.empty()) {
        newOp.id = generateUUID();
    }
    newOp.timestamp = std::chrono::system_clock::now();

    m_pendingOperations.push(newOp);

    if (m_persistenceEnabled) {
        savePendingOperations();
    }
}

void FirebaseCore::processPendingOperations() {
    if (!m_isOnline) return;

    std::lock_guard<std::mutex> lock(m_pendingOpsMutex);

    while (!m_pendingOperations.empty()) {
        PendingOperation op = m_pendingOperations.front();
        m_pendingOperations.pop();

        HttpRequest request;
        switch (op.type) {
            case PendingOperation::Type::Create:
                request.method = "POST";
                break;
            case PendingOperation::Type::Update:
                request.method = "PATCH";
                break;
            case PendingOperation::Type::Delete:
                request.method = "DELETE";
                break;
        }

        request.url = op.path;
        request.body = op.data;
        request.headers["Content-Type"] = "application/json";

        makeAuthenticatedRequest(request, [this, op](const HttpResponse& response) {
            if (response.statusCode >= 400) {
                // Re-queue failed operation with backoff
                if (op.retryCount < m_retryConfig.maxRetries) {
                    PendingOperation retryOp = op;
                    retryOp.retryCount++;
                    addPendingOperation(retryOp);
                }
            }
        });
    }
}

size_t FirebaseCore::getPendingOperationCount() const {
    std::lock_guard<std::mutex> lock(m_pendingOpsMutex);
    return m_pendingOperations.size();
}

void FirebaseCore::onError(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_errorCallbacks.push_back(callback);
}

void FirebaseCore::update(float deltaTime) {
    if (!m_initialized) return;

    // Token refresh check
    if (m_autoRefreshEnabled && m_isSignedIn && m_authToken.needsRefresh()) {
        refreshToken();
    }

    // Heartbeat for connection monitoring
    m_heartbeatTimer += deltaTime;
    if (m_heartbeatTimer >= 30.0f) {  // Every 30 seconds
        m_heartbeatTimer = 0.0f;

        if (m_isOnline && m_connectionState != ConnectionState::Connected) {
            reconnect();
        }
    }

    // Clean expired cache periodically
    static float cacheCleanTimer = 0.0f;
    cacheCleanTimer += deltaTime;
    if (cacheCleanTimer >= 300.0f) {  // Every 5 minutes
        cacheCleanTimer = 0.0f;
        cleanExpiredCache();
    }
}

// Private methods

void FirebaseCore::handleAuthResponse(const HttpResponse& response, AuthCallback callback) {
    if (response.statusCode == 200) {
        std::lock_guard<std::mutex> lock(m_authMutex);

        // Parse tokens
        m_authToken.idToken = extractJsonString(response.body, "idToken");
        m_authToken.refreshToken = extractJsonString(response.body, "refreshToken");

        int expiresIn = extractJsonInt(response.body, "expiresIn");
        if (expiresIn == 0) expiresIn = 3600;  // Default 1 hour
        m_authToken.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);

        // Parse user info
        parseUserFromResponse(response.body, m_currentUser);
        m_isSignedIn = true;

        updateConnectionState(ConnectionState::Connected);

        // Notify listeners
        {
            std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
            for (auto& cb : m_authCallbacks) {
                cb(m_currentUser, FirebaseError());
            }
        }

        if (callback) {
            callback(m_currentUser, FirebaseError());
        }
    } else {
        FirebaseError error = parseError(response);
        reportError(error);

        if (callback) {
            callback(FirebaseUser(), error);
        }
    }
}

void FirebaseCore::parseUserFromResponse(const std::string& json, FirebaseUser& user) {
    user.uid = extractJsonString(json, "localId");
    user.email = extractJsonString(json, "email");
    user.displayName = extractJsonString(json, "displayName");
    user.photoUrl = extractJsonString(json, "photoUrl");
    user.isAnonymous = user.email.empty();
    user.emailVerified = extractJsonBool(json, "emailVerified");

    // Determine provider
    std::string providerId = extractJsonString(json, "providerId");
    if (providerId == "google.com") {
        user.provider = AuthProvider::Google;
    } else if (providerId == "apple.com") {
        user.provider = AuthProvider::Apple;
    } else if (!user.email.empty()) {
        user.provider = AuthProvider::Email;
    } else {
        user.provider = AuthProvider::Anonymous;
    }
}

void FirebaseCore::executeHttpRequest(const HttpRequest& request, HttpCallback callback) {
    // This is a placeholder for actual HTTP implementation
    // In production, use libcurl, cpp-httplib, or platform-specific APIs

    // For now, simulate the request (this would be replaced with actual HTTP calls)
    HttpResponse response;

    // Platform-specific HTTP implementation would go here
    // Example with libcurl:
    /*
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());

        struct curl_slist* headers = nullptr;
        for (const auto& [key, value] : request.headers) {
            std::string header = key + ": " + value;
            headers = curl_slist_append(headers, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (!request.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        }

        std::string responseBody;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
            response.body = responseBody;
        } else {
            response.error.type = FirebaseErrorType::NetworkError;
            response.error.message = curl_easy_strerror(res);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    */

    // Simulated response for testing
    response.statusCode = 200;
    response.body = "{\"localId\":\"test123\",\"idToken\":\"mock_token\",\"refreshToken\":\"mock_refresh\",\"expiresIn\":\"3600\"}";

    if (callback) {
        callback(response);
    }
}

int FirebaseCore::calculateRetryDelay(int retryCount) {
    int delay = static_cast<int>(m_retryConfig.baseDelayMs * std::pow(m_retryConfig.backoffMultiplier, retryCount));
    delay = std::min(delay, m_retryConfig.maxDelayMs);

    if (m_retryConfig.useJitter) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, delay / 2);
        delay += dis(gen);
    }

    return delay;
}

void FirebaseCore::updateConnectionState(ConnectionState newState) {
    ConnectionState oldState = m_connectionState.exchange(newState);

    if (oldState != newState) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_connectionCallbacks) {
            callback(newState);
        }
    }
}

void FirebaseCore::loadCacheFromDisk() {
    std::ifstream file(m_cacheFilePath, std::ios::binary);
    if (!file.is_open()) return;

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();

    // Simple binary format: [keyLen][key][dataLen][data][timestamp][expiresAt]
    while (file.good()) {
        uint32_t keyLen;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        if (!file.good()) break;

        std::string key(keyLen, '\0');
        file.read(&key[0], keyLen);

        uint32_t dataLen;
        file.read(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));

        std::string data(dataLen, '\0');
        file.read(&data[0], dataLen);

        int64_t timestamp, expiresAt;
        file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        file.read(reinterpret_cast<char*>(&expiresAt), sizeof(expiresAt));

        CacheEntry entry;
        entry.key = key;
        entry.data = data;
        entry.timestamp = std::chrono::system_clock::from_time_t(timestamp);
        entry.expiresAt = std::chrono::system_clock::from_time_t(expiresAt);

        // Only load if not expired
        if (std::chrono::system_clock::now() < entry.expiresAt) {
            m_cache[key] = entry;
        }
    }
}

void FirebaseCore::saveCacheToDisk() {
    std::ofstream file(m_cacheFilePath, std::ios::binary);
    if (!file.is_open()) return;

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    for (const auto& [key, entry] : m_cache) {
        uint32_t keyLen = static_cast<uint32_t>(entry.key.size());
        file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        file.write(entry.key.c_str(), keyLen);

        uint32_t dataLen = static_cast<uint32_t>(entry.data.size());
        file.write(reinterpret_cast<const char*>(&dataLen), sizeof(dataLen));
        file.write(entry.data.c_str(), dataLen);

        int64_t timestamp = std::chrono::system_clock::to_time_t(entry.timestamp);
        int64_t expiresAt = std::chrono::system_clock::to_time_t(entry.expiresAt);
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        file.write(reinterpret_cast<const char*>(&expiresAt), sizeof(expiresAt));
    }
}

void FirebaseCore::loadPendingOperations() {
    std::ifstream file(m_pendingOpsFilePath, std::ios::binary);
    if (!file.is_open()) return;

    std::lock_guard<std::mutex> lock(m_pendingOpsMutex);

    while (file.good()) {
        int type;
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        if (!file.good()) break;

        uint32_t pathLen;
        file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));

        std::string path(pathLen, '\0');
        file.read(&path[0], pathLen);

        uint32_t dataLen;
        file.read(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));

        std::string data(dataLen, '\0');
        file.read(&data[0], dataLen);

        PendingOperation op;
        op.type = static_cast<PendingOperation::Type>(type);
        op.path = path;
        op.data = data;
        op.id = generateUUID();

        m_pendingOperations.push(op);
    }
}

void FirebaseCore::savePendingOperations() {
    std::ofstream file(m_pendingOpsFilePath, std::ios::binary);
    if (!file.is_open()) return;

    std::lock_guard<std::mutex> lock(m_pendingOpsMutex);

    // We need to temporarily dequeue and re-queue to save
    std::queue<PendingOperation> temp;

    while (!m_pendingOperations.empty()) {
        PendingOperation op = m_pendingOperations.front();
        m_pendingOperations.pop();
        temp.push(op);

        int type = static_cast<int>(op.type);
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));

        uint32_t pathLen = static_cast<uint32_t>(op.path.size());
        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(op.path.c_str(), pathLen);

        uint32_t dataLen = static_cast<uint32_t>(op.data.size());
        file.write(reinterpret_cast<const char*>(&dataLen), sizeof(dataLen));
        file.write(op.data.c_str(), dataLen);
    }

    m_pendingOperations = std::move(temp);
}

void FirebaseCore::cleanExpiredCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto now = std::chrono::system_clock::now();
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (now >= it->second.expiresAt) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

FirebaseError FirebaseCore::parseError(const HttpResponse& response) {
    FirebaseError error;

    // Parse error from response body
    std::string errorCode = extractJsonString(response.body, "error");
    std::string message = extractJsonString(response.body, "message");

    error.code = response.statusCode;
    error.message = message.empty() ? errorCode : message;
    error.details = response.body;

    // Map status codes to error types
    switch (response.statusCode) {
        case 400:
            error.type = FirebaseErrorType::InvalidArgument;
            break;
        case 401:
        case 403:
            error.type = FirebaseErrorType::AuthError;
            break;
        case 404:
            error.type = FirebaseErrorType::NotFound;
            break;
        case 409:
            error.type = FirebaseErrorType::AlreadyExists;
            break;
        case 429:
            error.type = FirebaseErrorType::RateLimited;
            break;
        case 500:
        case 502:
        case 503:
            error.type = FirebaseErrorType::ServerError;
            break;
        default:
            if (response.statusCode >= 400) {
                error.type = FirebaseErrorType::Unknown;
            }
            break;
    }

    // Check for network error
    if (response.error) {
        error = response.error;
    }

    return error;
}

void FirebaseCore::reportError(const FirebaseError& error) {
    m_lastError = error;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (auto& callback : m_errorCallbacks) {
        callback(error);
    }
}

bool m_autoRefreshEnabled = true;

} // namespace Firebase
} // namespace Network
