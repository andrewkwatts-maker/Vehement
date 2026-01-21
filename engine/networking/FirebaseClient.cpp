#include "FirebaseClient.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <future>
#include <vector>

// Platform-specific HTTP includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
    #define NOVA_HAS_WINHTTP 1
#elif defined(__APPLE__) || defined(__linux__)
    // libcurl would be used on these platforms
    // For now, stub implementations with TODO comments
    #define NOVA_HAS_WINHTTP 0
#else
    #define NOVA_HAS_WINHTTP 0
#endif

namespace Nova {

// Push ID characters (for Firebase-compatible push IDs)
static const char PUSH_CHARS[] = "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

FirebaseClient::FirebaseClient() = default;
FirebaseClient::~FirebaseClient() {
    Shutdown();
}

bool FirebaseClient::Initialize(const Config& config) {
    m_config = config;

    // Validate config
    if (config.databaseUrl.empty()) {
        return false;
    }

    // Load offline cache if enabled
    if (config.offlineEnabled) {
        LoadFromOfflineCache();
    }

    m_initialized = true;
    m_online = true;  // Assume online initially

    return true;
}

void FirebaseClient::Shutdown() {
    if (!m_initialized) return;

    // Save offline cache
    if (m_config.offlineEnabled) {
        SaveToOfflineCache();
    }

    // Clear listeners
    m_listeners.clear();

    // Clear queues
    while (!m_operationQueue.empty()) m_operationQueue.pop();
    while (!m_offlineQueue.empty()) m_offlineQueue.pop();

    m_initialized = false;
}

void FirebaseClient::Update(float deltaTime) {
    if (!m_initialized) return;

    // Process operation queue
    ProcessOperationQueue();

    // Poll listeners
    m_pollTimer += deltaTime;
    if (m_pollTimer >= m_config.pollInterval) {
        m_pollTimer = 0.0f;
        PollListeners();
    }

    // Refresh token if needed
    auto now = std::chrono::steady_clock::now();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    if (m_authState == FirebaseAuthState::Authenticated && nowMs > m_tokenExpiry - 300000) {
        // Token expires in < 5 minutes, refresh it
        RefreshToken();
    }
}

// ============================================================================
// Authentication
// ============================================================================

void FirebaseClient::SignInAnonymously(std::function<void(bool, const std::string&)> callback) {
    m_authState = FirebaseAuthState::Authenticating;

    std::string url = BuildAuthUrl("signInAnonymously");

    nlohmann::json body;
    body["returnSecureToken"] = true;

    HttpPost(url, body.dump(), [this, callback](int code, const std::string& response) {
        if (code == 200) {
            try {
                auto json = nlohmann::json::parse(response);
                m_userId = json["localId"].get<std::string>();
                m_idToken = json["idToken"].get<std::string>();
                m_refreshToken = json.value("refreshToken", "");

                int expiresIn = std::stoi(json.value("expiresIn", "3600"));
                auto now = std::chrono::steady_clock::now();
                m_tokenExpiry = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count() + expiresIn * 1000;

                m_authState = FirebaseAuthState::Authenticated;

                if (callback) callback(true, m_userId);
            } catch (...) {
                m_authState = FirebaseAuthState::Error;
                if (callback) callback(false, "Failed to parse auth response");
            }
        } else {
            m_authState = FirebaseAuthState::Error;
            if (callback) callback(false, "HTTP error: " + std::to_string(code));
        }
    });
}

void FirebaseClient::SignInWithEmail(const std::string& email, const std::string& password,
                                      std::function<void(bool, const std::string&)> callback) {
    m_authState = FirebaseAuthState::Authenticating;

    std::string url = BuildAuthUrl("signInWithPassword");

    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    body["returnSecureToken"] = true;

    HttpPost(url, body.dump(), [this, callback](int code, const std::string& response) {
        if (code == 200) {
            try {
                auto json = nlohmann::json::parse(response);
                m_userId = json["localId"].get<std::string>();
                m_idToken = json["idToken"].get<std::string>();
                m_refreshToken = json.value("refreshToken", "");

                int expiresIn = std::stoi(json.value("expiresIn", "3600"));
                auto now = std::chrono::steady_clock::now();
                m_tokenExpiry = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count() + expiresIn * 1000;

                m_authState = FirebaseAuthState::Authenticated;

                if (callback) callback(true, m_userId);
            } catch (...) {
                m_authState = FirebaseAuthState::Error;
                if (callback) callback(false, "Failed to parse auth response");
            }
        } else {
            m_authState = FirebaseAuthState::Error;
            if (callback) callback(false, "Authentication failed");
        }
    });
}

void FirebaseClient::SignInWithCustomToken(const std::string& token,
                                            std::function<void(bool, const std::string&)> callback) {
    m_authState = FirebaseAuthState::Authenticating;

    std::string url = BuildAuthUrl("signInWithCustomToken");

    nlohmann::json body;
    body["token"] = token;
    body["returnSecureToken"] = true;

    HttpPost(url, body.dump(), [this, callback](int code, const std::string& response) {
        if (code == 200) {
            try {
                auto json = nlohmann::json::parse(response);
                m_userId = json["localId"].get<std::string>();
                m_idToken = json["idToken"].get<std::string>();

                m_authState = FirebaseAuthState::Authenticated;

                if (callback) callback(true, m_userId);
            } catch (...) {
                m_authState = FirebaseAuthState::Error;
                if (callback) callback(false, "Failed to parse auth response");
            }
        } else {
            m_authState = FirebaseAuthState::Error;
            if (callback) callback(false, "Authentication failed");
        }
    });
}

void FirebaseClient::SignOut() {
    m_authState = FirebaseAuthState::NotAuthenticated;
    m_userId.clear();
    m_idToken.clear();
    m_refreshToken.clear();
    m_tokenExpiry = 0;
}

void FirebaseClient::RefreshToken() {
    // Refresh the auth token using the refresh token
    // This calls the Firebase Auth securetoken endpoint

    if (m_refreshToken.empty()) {
        m_authState = FirebaseAuthState::NotAuthenticated;
        return;
    }

    std::string url = "https://securetoken.googleapis.com/v1/token?key=" + m_config.apiKey;

    nlohmann::json body;
    body["grant_type"] = "refresh_token";
    body["refresh_token"] = m_refreshToken;

    HttpPost(url, body.dump(), [this](int code, const std::string& response) {
        if (code == 200) {
            try {
                auto json = nlohmann::json::parse(response);

                // The response uses different field names than the sign-in response
                m_idToken = json.value("id_token", json.value("idToken", ""));
                m_refreshToken = json.value("refresh_token", json.value("refreshToken", m_refreshToken));

                // expires_in is in seconds
                int expiresIn = 3600;
                if (json.contains("expires_in")) {
                    expiresIn = std::stoi(json["expires_in"].get<std::string>());
                }

                auto now = std::chrono::steady_clock::now();
                m_tokenExpiry = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count() + expiresIn * 1000;

                // Token refreshed successfully, stay authenticated
                m_authState = FirebaseAuthState::Authenticated;

            } catch (const std::exception& e) {
                // Failed to parse response, but don't change auth state
                // Token may still be valid
            }
        } else {
            // Refresh failed - token might be revoked
            // Don't immediately sign out, let the next request fail naturally
        }
    });
}

// ============================================================================
// Database Operations
// ============================================================================

void FirebaseClient::Get(const std::string& path, std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Get;
    op.path = path;
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

void FirebaseClient::Set(const std::string& path, const nlohmann::json& data,
                          std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Set;
    op.path = path;
    op.data = data;
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

void FirebaseClient::Update(const std::string& path, const nlohmann::json& data,
                             std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Update;
    op.path = path;
    op.data = data;
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

void FirebaseClient::Push(const std::string& path, const nlohmann::json& data,
                           std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Push;
    op.path = path;
    op.data = data;
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

void FirebaseClient::Delete(const std::string& path, std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Delete;
    op.path = path;
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

void FirebaseClient::Transaction(const std::string& path,
                                  std::function<nlohmann::json(const nlohmann::json&)> updateFunc,
                                  std::function<void(const FirebaseResult&)> callback) {
    FirebaseOperation op;
    op.type = FirebaseOperation::Type::Transaction;
    op.path = path;
    op.transactionFunc = std::move(updateFunc);
    op.callback = std::move(callback);
    QueueOperation(std::move(op));
}

// ============================================================================
// Listeners
// ============================================================================

uint64_t FirebaseClient::AddValueListener(const std::string& path,
                                           std::function<void(const nlohmann::json&)> callback) {
    FirebaseListener listener;
    listener.id = m_nextListenerId++;
    listener.path = path;
    listener.onValue = std::move(callback);
    m_listeners.push_back(std::move(listener));

    // Initial fetch
    Get(path, [this, id = listener.id](const FirebaseResult& result) {
        for (auto& l : m_listeners) {
            if (l.id == id && l.onValue && result.success) {
                m_listenerCache[l.path] = result.data;
                l.onValue(result.data);
            }
        }
    });

    return listener.id;
}

uint64_t FirebaseClient::AddChildListener(const std::string& path,
                                           std::function<void(const std::string&, const nlohmann::json&)> onAdded,
                                           std::function<void(const std::string&, const nlohmann::json&)> onChanged,
                                           std::function<void(const std::string&)> onRemoved) {
    FirebaseListener listener;
    listener.id = m_nextListenerId++;
    listener.path = path;
    listener.onChildAdded = std::move(onAdded);
    listener.onChildChanged = std::move(onChanged);
    listener.onChildRemoved = std::move(onRemoved);
    m_listeners.push_back(std::move(listener));

    return listener.id;
}

void FirebaseClient::RemoveListener(uint64_t listenerId) {
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
            [listenerId](const FirebaseListener& l) { return l.id == listenerId; }),
        m_listeners.end()
    );
}

// ============================================================================
// Terrain Persistence
// ============================================================================

void FirebaseClient::SaveTerrainChunk(const std::string& worldId, int chunkX, int chunkY, int chunkZ,
                                       const nlohmann::json& chunkData,
                                       std::function<void(bool)> callback) {
    std::stringstream path;
    path << "worlds/" << worldId << "/chunks/" << chunkX << "_" << chunkY << "_" << chunkZ;

    Set(path.str(), chunkData, [callback](const FirebaseResult& result) {
        if (callback) callback(result.success);
    });
}

void FirebaseClient::LoadTerrainChunk(const std::string& worldId, int chunkX, int chunkY, int chunkZ,
                                       std::function<void(bool, const nlohmann::json&)> callback) {
    std::stringstream path;
    path << "worlds/" << worldId << "/chunks/" << chunkX << "_" << chunkY << "_" << chunkZ;

    Get(path.str(), [callback](const FirebaseResult& result) {
        if (callback) callback(result.success, result.data);
    });
}

void FirebaseClient::SaveTerrainModification(const std::string& worldId,
                                              const nlohmann::json& modification,
                                              std::function<void(bool)> callback) {
    std::string path = "worlds/" + worldId + "/modifications";

    nlohmann::json modWithTimestamp = modification;
    modWithTimestamp["timestamp"] = ServerTimestamp();

    Push(path, modWithTimestamp, [callback](const FirebaseResult& result) {
        if (callback) callback(result.success);
    });
}

void FirebaseClient::LoadTerrainModifications(const std::string& worldId, uint64_t sinceTimestamp,
                                               std::function<void(const std::vector<nlohmann::json>&)> callback) {
    std::string path = "worlds/" + worldId + "/modifications";

    // In a real implementation, we'd use query parameters to filter by timestamp
    Get(path, [callback, sinceTimestamp](const FirebaseResult& result) {
        std::vector<nlohmann::json> modifications;

        if (result.success && result.data.is_object()) {
            for (auto& [key, value] : result.data.items()) {
                uint64_t ts = value.value("timestamp", 0ULL);
                if (ts > sinceTimestamp) {
                    modifications.push_back(value);
                }
            }

            // Sort by timestamp
            std::sort(modifications.begin(), modifications.end(),
                [](const nlohmann::json& a, const nlohmann::json& b) {
                    return a.value("timestamp", 0ULL) < b.value("timestamp", 0ULL);
                });
        }

        if (callback) callback(modifications);
    });
}

uint64_t FirebaseClient::SubscribeToTerrainModifications(const std::string& worldId,
                                                          std::function<void(const nlohmann::json&)> callback) {
    std::string path = "worlds/" + worldId + "/modifications";

    return AddChildListener(path,
        [callback](const std::string&, const nlohmann::json& data) {
            if (callback) callback(data);
        },
        nullptr, nullptr
    );
}

// ============================================================================
// Offline Support
// ============================================================================

size_t FirebaseClient::GetPendingOperationCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_operationQueue.size() + m_offlineQueue.size();
}

void FirebaseClient::SyncOfflineOperations() {
    if (!m_online) return;

    std::lock_guard<std::mutex> lock(m_queueMutex);

    while (!m_offlineQueue.empty()) {
        m_operationQueue.push(std::move(m_offlineQueue.front()));
        m_offlineQueue.pop();
    }
}

void FirebaseClient::ClearOfflineCache() {
    m_offlineCache.clear();
    while (!m_offlineQueue.empty()) m_offlineQueue.pop();

    // Delete cache file
    std::remove(m_config.offlineCachePath.c_str());
}

// ============================================================================
// Utility
// ============================================================================

nlohmann::json FirebaseClient::ServerTimestamp() {
    return nlohmann::json{{".sv", "timestamp"}};
}

std::string FirebaseClient::GeneratePushId() {
    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::string id;
    id.reserve(20);

    // Encode timestamp in first 8 characters
    for (int i = 7; i >= 0; i--) {
        id += PUSH_CHARS[static_cast<int>(timestamp % 64)];
        timestamp /= 64;
    }
    std::reverse(id.begin(), id.end());

    // Add random characters
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 63);

    for (int i = 0; i < 12; i++) {
        id += PUSH_CHARS[dis(gen)];
    }

    m_lastPushId = id;
    return id;
}

// ============================================================================
// Private Methods
// ============================================================================

#if NOVA_HAS_WINHTTP

// WinHTTP helper: Parse URL into components
namespace {
    struct UrlComponents {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port;
        bool secure;
    };

    bool ParseUrl(const std::string& url, UrlComponents& components) {
        // Convert to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
        std::wstring wurl(wlen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], wlen);

        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t hostBuf[256] = {};
        wchar_t pathBuf[2048] = {};

        urlComp.lpszHostName = hostBuf;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = pathBuf;
        urlComp.dwUrlPathLength = 2048;

        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
            return false;
        }

        components.host = hostBuf;
        components.path = pathBuf;
        components.port = urlComp.nPort;
        components.secure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

        return true;
    }

    // Perform HTTP request using WinHTTP
    void WinHttpRequest(const std::string& url,
                        const std::wstring& method,
                        const std::string& body,
                        std::function<void(int, const std::string&)> callback) {
        // Run in a background thread to avoid blocking
        std::thread([url, method, body, callback]() {
            int statusCode = 0;
            std::string responseBody;

            UrlComponents components;
            if (!ParseUrl(url, components)) {
                if (callback) callback(0, "Failed to parse URL");
                return;
            }

            // Open WinHTTP session
            HINTERNET hSession = WinHttpOpen(L"Nova3D/1.0",
                                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                              WINHTTP_NO_PROXY_NAME,
                                              WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) {
                if (callback) callback(0, "Failed to open WinHTTP session");
                return;
            }

            // Connect to server
            HINTERNET hConnect = WinHttpConnect(hSession,
                                                 components.host.c_str(),
                                                 components.port, 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                if (callback) callback(0, "Failed to connect");
                return;
            }

            // Create request
            DWORD flags = components.secure ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                                     method.c_str(),
                                                     components.path.c_str(),
                                                     nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                     flags);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                if (callback) callback(0, "Failed to create request");
                return;
            }

            // Set content-type header for requests with body
            if (!body.empty()) {
                WinHttpAddRequestHeaders(hRequest,
                    L"Content-Type: application/json",
                    -1, WINHTTP_ADDREQ_FLAG_ADD);
            }

            // Send request
            BOOL result = WinHttpSendRequest(hRequest,
                                              WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                              body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
                                              static_cast<DWORD>(body.size()),
                                              static_cast<DWORD>(body.size()),
                                              0);

            if (result) {
                result = WinHttpReceiveResponse(hRequest, nullptr);
            }

            if (result) {
                // Get status code
                DWORD statusCodeSize = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest,
                                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    &statusCode, &statusCodeSize,
                                    WINHTTP_NO_HEADER_INDEX);

                // Read response body
                DWORD bytesAvailable = 0;
                do {
                    bytesAvailable = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                        break;
                    }

                    if (bytesAvailable > 0) {
                        std::vector<char> buffer(bytesAvailable + 1);
                        DWORD bytesRead = 0;
                        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                            responseBody.append(buffer.data(), bytesRead);
                        }
                    }
                } while (bytesAvailable > 0);
            }

            // Cleanup
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            if (callback) {
                callback(statusCode, responseBody);
            }
        }).detach();
    }
}

void FirebaseClient::HttpGet(const std::string& url, std::function<void(int, const std::string&)> callback) {
    WinHttpRequest(url, L"GET", "", callback);
}

void FirebaseClient::HttpPost(const std::string& url, const std::string& body,
                               std::function<void(int, const std::string&)> callback) {
    WinHttpRequest(url, L"POST", body, callback);
}

void FirebaseClient::HttpPut(const std::string& url, const std::string& body,
                              std::function<void(int, const std::string&)> callback) {
    WinHttpRequest(url, L"PUT", body, callback);
}

void FirebaseClient::HttpPatch(const std::string& url, const std::string& body,
                                std::function<void(int, const std::string&)> callback) {
    WinHttpRequest(url, L"PATCH", body, callback);
}

void FirebaseClient::HttpDelete(const std::string& url, std::function<void(int, const std::string&)> callback) {
    WinHttpRequest(url, L"DELETE", "", callback);
}

#else // !NOVA_HAS_WINHTTP

// Stub implementations for non-Windows platforms
// TODO: Implement using libcurl when NOVA_ENABLE_NETWORKING includes curl dependency

void FirebaseClient::HttpGet(const std::string& url, std::function<void(int, const std::string&)> callback) {
    // Stub: Requires libcurl integration on non-Windows platforms
    // To enable: add curl dependency to CMakeLists.txt and implement using curl_easy_* API
    if (callback) {
        callback(501, R"({"error": "HTTP not implemented on this platform - requires libcurl"})");
    }
}

void FirebaseClient::HttpPost(const std::string& url, const std::string& body,
                               std::function<void(int, const std::string&)> callback) {
    // Stub: Requires libcurl integration on non-Windows platforms
    if (callback) {
        callback(501, R"({"error": "HTTP not implemented on this platform - requires libcurl"})");
    }
}

void FirebaseClient::HttpPut(const std::string& url, const std::string& body,
                              std::function<void(int, const std::string&)> callback) {
    // Stub: Requires libcurl integration on non-Windows platforms
    if (callback) {
        callback(501, R"({"error": "HTTP not implemented on this platform - requires libcurl"})");
    }
}

void FirebaseClient::HttpPatch(const std::string& url, const std::string& body,
                                std::function<void(int, const std::string&)> callback) {
    // Stub: Requires libcurl integration on non-Windows platforms
    if (callback) {
        callback(501, R"({"error": "HTTP not implemented on this platform - requires libcurl"})");
    }
}

void FirebaseClient::HttpDelete(const std::string& url, std::function<void(int, const std::string&)> callback) {
    // Stub: Requires libcurl integration on non-Windows platforms
    if (callback) {
        callback(501, R"({"error": "HTTP not implemented on this platform - requires libcurl"})");
    }
}

#endif // NOVA_HAS_WINHTTP

std::string FirebaseClient::BuildDatabaseUrl(const std::string& path) const {
    std::string url = m_config.databaseUrl;
    if (!url.empty() && url.back() != '/') {
        url += '/';
    }
    url += path + ".json";

    // Add auth token if available
    if (!m_idToken.empty()) {
        url += "?auth=" + m_idToken;
    }

    return url;
}

std::string FirebaseClient::BuildAuthUrl(const std::string& endpoint) const {
    return "https://identitytoolkit.googleapis.com/v1/accounts:" + endpoint +
           "?key=" + m_config.apiKey;
}

void FirebaseClient::ProcessOperationQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    int processed = 0;
    const int maxPerFrame = 5;

    while (!m_operationQueue.empty() && processed < maxPerFrame) {
        FirebaseOperation op = std::move(m_operationQueue.front());
        m_operationQueue.pop();
        processed++;

        if (!m_online && m_config.offlineEnabled) {
            // Queue for later
            m_offlineQueue.push(std::move(op));
            continue;
        }

        std::string url = BuildDatabaseUrl(op.path);

        switch (op.type) {
            case FirebaseOperation::Type::Get:
                HttpGet(url, [op](int code, const std::string& response) {
                    if (op.callback) {
                        FirebaseResult result;
                        result.success = (code == 200);
                        result.httpCode = code;
                        if (result.success) {
                            try {
                                result.data = nlohmann::json::parse(response);
                            } catch (...) {
                                result.success = false;
                                result.errorMessage = "JSON parse error";
                            }
                        }
                        op.callback(result);
                    }
                });
                break;

            case FirebaseOperation::Type::Set:
                HttpPut(url, op.data.dump(), [op](int code, const std::string& response) {
                    if (op.callback) {
                        FirebaseResult result;
                        result.success = (code == 200);
                        result.httpCode = code;
                        op.callback(result);
                    }
                });
                break;

            case FirebaseOperation::Type::Update:
                HttpPatch(url, op.data.dump(), [op](int code, const std::string& response) {
                    if (op.callback) {
                        FirebaseResult result;
                        result.success = (code == 200);
                        result.httpCode = code;
                        op.callback(result);
                    }
                });
                break;

            case FirebaseOperation::Type::Push: {
                std::string pushId = GeneratePushId();
                std::string pushUrl = BuildDatabaseUrl(op.path + "/" + pushId);
                HttpPut(pushUrl, op.data.dump(), [op, pushId](int code, const std::string& response) {
                    if (op.callback) {
                        FirebaseResult result;
                        result.success = (code == 200);
                        result.httpCode = code;
                        result.data["name"] = pushId;
                        op.callback(result);
                    }
                });
                break;
            }

            case FirebaseOperation::Type::Delete:
                HttpDelete(url, [op](int code, const std::string& response) {
                    if (op.callback) {
                        FirebaseResult result;
                        result.success = (code == 200);
                        result.httpCode = code;
                        op.callback(result);
                    }
                });
                break;

            case FirebaseOperation::Type::Transaction:
                // First get current value
                HttpGet(url, [this, op, url](int code, const std::string& response) mutable {
                    if (code == 200 && op.transactionFunc) {
                        try {
                            nlohmann::json current = nlohmann::json::parse(response);
                            nlohmann::json updated = op.transactionFunc(current);

                            HttpPut(url, updated.dump(), [op](int code2, const std::string&) {
                                if (op.callback) {
                                    FirebaseResult result;
                                    result.success = (code2 == 200);
                                    result.httpCode = code2;
                                    op.callback(result);
                                }
                            });
                        } catch (...) {
                            if (op.callback) {
                                FirebaseResult result;
                                result.success = false;
                                result.errorMessage = "Transaction failed";
                                op.callback(result);
                            }
                        }
                    }
                });
                break;
        }
    }
}

void FirebaseClient::QueueOperation(FirebaseOperation op) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    if (m_operationQueue.size() >= static_cast<size_t>(m_config.maxQueueSize)) {
        // Queue full, drop oldest
        m_operationQueue.pop();
    }

    m_operationQueue.push(std::move(op));
}

void FirebaseClient::RetryOperation(FirebaseOperation& op) {
    if (op.retryCount < FirebaseOperation::maxRetries) {
        op.retryCount++;
        QueueOperation(std::move(op));
    } else if (op.callback) {
        FirebaseResult result;
        result.success = false;
        result.errorMessage = "Max retries exceeded";
        op.callback(result);
    }
}

void FirebaseClient::PollListeners() {
    for (auto& listener : m_listeners) {
        if (!listener.active) continue;

        Get(listener.path, [this, &listener](const FirebaseResult& result) {
            if (!result.success) {
                if (listener.onError) {
                    listener.onError(result.errorMessage);
                }
                return;
            }

            auto it = m_listenerCache.find(listener.path);
            bool isNew = (it == m_listenerCache.end());
            nlohmann::json oldValue = isNew ? nlohmann::json() : it->second;
            nlohmann::json newValue = result.data;

            // Update cache
            m_listenerCache[listener.path] = newValue;

            // Value listener
            if (listener.onValue && (isNew || oldValue != newValue)) {
                listener.onValue(newValue);
            }

            // Child listeners
            if ((listener.onChildAdded || listener.onChildChanged || listener.onChildRemoved) &&
                newValue.is_object()) {

                std::unordered_set<std::string> oldKeys;
                if (oldValue.is_object()) {
                    for (auto& [key, val] : oldValue.items()) {
                        oldKeys.insert(key);
                    }
                }

                for (auto& [key, val] : newValue.items()) {
                    if (oldKeys.find(key) == oldKeys.end()) {
                        // New child
                        if (listener.onChildAdded) {
                            listener.onChildAdded(key, val);
                        }
                    } else if (oldValue.is_object() && oldValue[key] != val) {
                        // Changed child
                        if (listener.onChildChanged) {
                            listener.onChildChanged(key, val);
                        }
                    }
                    oldKeys.erase(key);
                }

                // Removed children
                for (const auto& key : oldKeys) {
                    if (listener.onChildRemoved) {
                        listener.onChildRemoved(key);
                    }
                }
            }
        });
    }
}

void FirebaseClient::SaveToOfflineCache() {
    nlohmann::json cache;
    cache["data"] = m_offlineCache;

    nlohmann::json queueData = nlohmann::json::array();
    std::queue<FirebaseOperation> tempQueue = m_offlineQueue;
    while (!tempQueue.empty()) {
        FirebaseOperation& op = tempQueue.front();
        nlohmann::json opJson;
        opJson["type"] = static_cast<int>(op.type);
        opJson["path"] = op.path;
        opJson["data"] = op.data;
        queueData.push_back(opJson);
        tempQueue.pop();
    }
    cache["queue"] = queueData;

    std::ofstream file(m_config.offlineCachePath);
    if (file.is_open()) {
        file << cache.dump(2);
    }
}

void FirebaseClient::LoadFromOfflineCache() {
    std::ifstream file(m_config.offlineCachePath);
    if (!file.is_open()) return;

    try {
        nlohmann::json cache;
        file >> cache;

        if (cache.contains("data")) {
            m_offlineCache = cache["data"].get<std::unordered_map<std::string, nlohmann::json>>();
        }

        if (cache.contains("queue") && cache["queue"].is_array()) {
            for (const auto& opJson : cache["queue"]) {
                FirebaseOperation op;
                op.type = static_cast<FirebaseOperation::Type>(opJson.value("type", 0));
                op.path = opJson.value("path", "");
                op.data = opJson.value("data", nlohmann::json());
                m_offlineQueue.push(std::move(op));
            }
        }
    } catch (...) {
        // Invalid cache, ignore
    }
}

// ============================================================================
// FirebaseQuery Implementation
// ============================================================================

FirebaseQuery::FirebaseQuery(FirebaseClient* client, const std::string& path)
    : m_client(client), m_path(path) {}

FirebaseQuery& FirebaseQuery::OrderByChild(const std::string& child) {
    m_orderBy = "\"" + child + "\"";
    return *this;
}

FirebaseQuery& FirebaseQuery::OrderByKey() {
    m_orderBy = "\"$key\"";
    return *this;
}

FirebaseQuery& FirebaseQuery::OrderByValue() {
    m_orderBy = "\"$value\"";
    return *this;
}

FirebaseQuery& FirebaseQuery::StartAt(const nlohmann::json& value) {
    m_startAt = value;
    return *this;
}

FirebaseQuery& FirebaseQuery::EndAt(const nlohmann::json& value) {
    m_endAt = value;
    return *this;
}

FirebaseQuery& FirebaseQuery::EqualTo(const nlohmann::json& value) {
    m_equalTo = value;
    return *this;
}

FirebaseQuery& FirebaseQuery::LimitToFirst(int limit) {
    m_limitToFirst = limit;
    return *this;
}

FirebaseQuery& FirebaseQuery::LimitToLast(int limit) {
    m_limitToLast = limit;
    return *this;
}

void FirebaseQuery::Get(std::function<void(const FirebaseResult&)> callback) {
    // Build query string and append to path
    // For now, just do regular get
    m_client->Get(m_path, std::move(callback));
}

uint64_t FirebaseQuery::Listen(std::function<void(const nlohmann::json&)> callback) {
    return m_client->AddValueListener(m_path, std::move(callback));
}

std::string FirebaseQuery::BuildQueryString() const {
    std::stringstream ss;
    bool first = true;

    auto addParam = [&ss, &first](const std::string& name, const std::string& value) {
        ss << (first ? "?" : "&") << name << "=" << value;
        first = false;
    };

    if (!m_orderBy.empty()) {
        addParam("orderBy", m_orderBy);
    }
    if (m_startAt.has_value()) {
        addParam("startAt", m_startAt->dump());
    }
    if (m_endAt.has_value()) {
        addParam("endAt", m_endAt->dump());
    }
    if (m_equalTo.has_value()) {
        addParam("equalTo", m_equalTo->dump());
    }
    if (m_limitToFirst > 0) {
        addParam("limitToFirst", std::to_string(m_limitToFirst));
    }
    if (m_limitToLast > 0) {
        addParam("limitToLast", std::to_string(m_limitToLast));
    }

    return ss.str();
}

} // namespace Nova
