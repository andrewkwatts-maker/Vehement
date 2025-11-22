#include "FirebaseManager.hpp"
#include <fstream>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

// Include engine logger if available
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define FIREBASE_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define FIREBASE_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define FIREBASE_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define FIREBASE_LOG_INFO(...) std::cout << "[Firebase] " << __VA_ARGS__ << std::endl
#define FIREBASE_LOG_WARN(...) std::cerr << "[Firebase WARN] " << __VA_ARGS__ << std::endl
#define FIREBASE_LOG_ERROR(...) std::cerr << "[Firebase ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

FirebaseManager& FirebaseManager::Instance() {
    static FirebaseManager instance;
    return instance;
}

FirebaseManager::~FirebaseManager() {
    if (m_initialized) {
        Shutdown();
    }
}

bool FirebaseManager::Initialize(const std::string& configPath) {
    if (m_initialized) {
        FIREBASE_LOG_WARN("FirebaseManager already initialized");
        return true;
    }

    if (!LoadConfig(configPath)) {
        FIREBASE_LOG_ERROR("Failed to load Firebase config from: " + configPath);
        return false;
    }

    m_initialized = true;
    m_connectionState = ConnectionState::Connected; // Stub: assume connected
    m_offlineMode = true; // Stub implementation runs in offline mode

    FIREBASE_LOG_INFO("FirebaseManager initialized (offline stub mode)");
    NotifyConnectionStateChanged(ConnectionState::Connected);

    return true;
}

bool FirebaseManager::Initialize(const FirebaseConfig& config) {
    if (m_initialized) {
        FIREBASE_LOG_WARN("FirebaseManager already initialized");
        return true;
    }

    m_config = config;
    m_initialized = true;
    m_connectionState = ConnectionState::Connected;
    m_offlineMode = true;

    FIREBASE_LOG_INFO("FirebaseManager initialized with config (offline stub mode)");
    NotifyConnectionStateChanged(ConnectionState::Connected);

    return true;
}

void FirebaseManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    FIREBASE_LOG_INFO("Shutting down FirebaseManager");

    // Stop all listeners
    StopAllListeners();

    // Sign out
    SignOut();

    // Clear local data
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_localData.clear();
    }

    // Clear pending operations
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        while (!m_pendingOperations.empty()) {
            m_pendingOperations.pop();
        }
    }

    m_connectionState = ConnectionState::Disconnected;
    NotifyConnectionStateChanged(ConnectionState::Disconnected);

    m_initialized = false;
}

void FirebaseManager::OnConnectionStateChanged(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(m_connectionCallbackMutex);
    m_connectionCallbacks.push_back(std::move(callback));
}

// ==================== Authentication ====================

void FirebaseManager::SignInAnonymously(AuthCallback callback) {
    if (!m_initialized) {
        if (callback) {
            callback(false, "");
        }
        return;
    }

    // Stub implementation: generate a random user ID
    SimulateNetworkDelay([this, callback]() {
        std::string userId = "anon_" + GenerateUniqueId();

        {
            std::lock_guard<std::mutex> lock(m_authMutex);
            m_userId = userId;
        }

        FIREBASE_LOG_INFO("Anonymous sign-in successful. User ID: " + userId);

        if (callback) {
            callback(true, userId);
        }
    });
}

void FirebaseManager::SignOut() {
    std::lock_guard<std::mutex> lock(m_authMutex);
    if (!m_userId.empty()) {
        FIREBASE_LOG_INFO("User signed out: " + m_userId);
        m_userId.clear();
    }
}

std::string FirebaseManager::GetUserId() const {
    std::lock_guard<std::mutex> lock(m_authMutex);
    return m_userId;
}

// ==================== Realtime Database ====================

void FirebaseManager::SetValue(const std::string& path, const nlohmann::json& value,
                                ResultCallback callback) {
    if (!m_initialized) {
        if (callback) {
            callback({false, ErrorCode::NotInitialized, "FirebaseManager not initialized"});
        }
        return;
    }

    // Store locally
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_localData[path] = value;
    }

    // Notify listeners
    NotifyListeners(path, value);

    FIREBASE_LOG_INFO("SetValue at path: " + path);

    if (callback) {
        callback({true, ErrorCode::None, ""});
    }
}

void FirebaseManager::GetValue(const std::string& path, DataCallback callback) {
    if (!m_initialized) {
        if (callback) {
            callback(nlohmann::json::object());
        }
        return;
    }

    nlohmann::json data;
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        auto it = m_localData.find(path);
        if (it != m_localData.end()) {
            data = it->second;
        }
    }

    if (callback) {
        callback(data);
    }
}

void FirebaseManager::UpdateValue(const std::string& path, const nlohmann::json& updates,
                                   ResultCallback callback) {
    if (!m_initialized) {
        if (callback) {
            callback({false, ErrorCode::NotInitialized, "FirebaseManager not initialized"});
        }
        return;
    }

    // Merge updates with existing data
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        auto it = m_localData.find(path);
        if (it != m_localData.end()) {
            // Merge: existing data with updates
            for (auto& [key, val] : updates.items()) {
                it->second[key] = val;
            }
        } else {
            m_localData[path] = updates;
        }

        // Notify listeners
        NotifyListeners(path, m_localData[path]);
    }

    FIREBASE_LOG_INFO("UpdateValue at path: " + path);

    if (callback) {
        callback({true, ErrorCode::None, ""});
    }
}

void FirebaseManager::DeleteValue(const std::string& path, ResultCallback callback) {
    if (!m_initialized) {
        if (callback) {
            callback({false, ErrorCode::NotInitialized, "FirebaseManager not initialized"});
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_localData.erase(path);
    }

    // Notify listeners with null/empty
    NotifyListeners(path, nlohmann::json(nullptr));

    FIREBASE_LOG_INFO("DeleteValue at path: " + path);

    if (callback) {
        callback({true, ErrorCode::None, ""});
    }
}

std::string FirebaseManager::PushValue(const std::string& path, const nlohmann::json& value) {
    std::string key = GenerateUniqueId();
    std::string fullPath = path + "/" + key;

    SetValue(fullPath, value);

    return key;
}

// ==================== Real-time Listeners ====================

std::string FirebaseManager::ListenToPath(const std::string& path, DataCallback callback) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);

    std::string listenerId = "listener_" + std::to_string(m_nextListenerId++);

    m_listeners[listenerId] = {path, callback, true};

    FIREBASE_LOG_INFO("Started listening to path: " + path + " (ID: " + listenerId + ")");

    // Immediately invoke with current data
    nlohmann::json currentData;
    {
        std::lock_guard<std::mutex> dataLock(m_dataMutex);
        auto it = m_localData.find(path);
        if (it != m_localData.end()) {
            currentData = it->second;
        }
    }

    if (callback) {
        callback(currentData);
    }

    return listenerId;
}

void FirebaseManager::StopListening(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);

    for (auto it = m_listeners.begin(); it != m_listeners.end();) {
        if (it->second.path == path) {
            FIREBASE_LOG_INFO("Stopped listening to path: " + path);
            it = m_listeners.erase(it);
        } else {
            ++it;
        }
    }
}

void FirebaseManager::StopListeningById(const std::string& listenerId) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);

    auto it = m_listeners.find(listenerId);
    if (it != m_listeners.end()) {
        FIREBASE_LOG_INFO("Stopped listener: " + listenerId);
        m_listeners.erase(it);
    }
}

void FirebaseManager::StopAllListeners() {
    std::lock_guard<std::mutex> lock(m_listenerMutex);

    FIREBASE_LOG_INFO("Stopping all listeners (" + std::to_string(m_listeners.size()) + " active)");
    m_listeners.clear();
}

// ==================== Cloud Firestore ====================

void FirebaseManager::GetDocument(const std::string& collection, const std::string& documentId,
                                   DataCallback callback) {
    std::string path = "firestore/" + collection + "/" + documentId;
    GetValue(path, callback);
}

void FirebaseManager::SetDocument(const std::string& collection, const std::string& documentId,
                                   const nlohmann::json& data, bool merge,
                                   ResultCallback callback) {
    std::string path = "firestore/" + collection + "/" + documentId;

    if (merge) {
        UpdateValue(path, data, callback);
    } else {
        SetValue(path, data, callback);
    }
}

void FirebaseManager::QueryDocuments(const std::string& collection,
                                      const std::string& field,
                                      const std::string& op,
                                      const nlohmann::json& value,
                                      DataCallback callback) {
    // Stub implementation: scan local data for matching documents
    nlohmann::json results = nlohmann::json::array();

    std::string prefix = "firestore/" + collection + "/";

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        for (const auto& [path, data] : m_localData) {
            if (path.find(prefix) == 0 && data.contains(field)) {
                bool matches = false;

                // Simple comparison operators
                if (op == "==") {
                    matches = (data[field] == value);
                } else if (op == "!=") {
                    matches = (data[field] != value);
                } else if (op == "<") {
                    matches = (data[field] < value);
                } else if (op == ">") {
                    matches = (data[field] > value);
                } else if (op == "<=") {
                    matches = (data[field] <= value);
                } else if (op == ">=") {
                    matches = (data[field] >= value);
                }

                if (matches) {
                    nlohmann::json doc = data;
                    doc["_id"] = path.substr(prefix.length());
                    results.push_back(doc);
                }
            }
        }
    }

    if (callback) {
        callback(results);
    }
}

// ==================== Offline Support ====================

void FirebaseManager::EnableOfflineMode() {
    m_offlineMode = true;
    FIREBASE_LOG_INFO("Offline mode enabled");
}

void FirebaseManager::Update() {
    // Process pending operations
    std::queue<PendingOperation> toProcess;

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        std::swap(toProcess, m_pendingOperations);
    }

    // In a real implementation, this would attempt to sync with the server
    // For the stub, we just process locally
    while (!toProcess.empty()) {
        auto& op = toProcess.front();

        switch (op.type) {
            case PendingOperation::Type::Set:
                SetValue(op.path, op.data, op.callback);
                break;
            case PendingOperation::Type::Update:
                UpdateValue(op.path, op.data, op.callback);
                break;
            case PendingOperation::Type::Delete:
                DeleteValue(op.path, op.callback);
                break;
            case PendingOperation::Type::Push:
                PushValue(op.path, op.data);
                if (op.callback) {
                    op.callback({true, ErrorCode::None, ""});
                }
                break;
        }

        toProcess.pop();
    }
}

// ==================== Private Helpers ====================

void FirebaseManager::NotifyConnectionStateChanged(ConnectionState state) {
    std::lock_guard<std::mutex> lock(m_connectionCallbackMutex);
    for (auto& callback : m_connectionCallbacks) {
        if (callback) {
            callback(state);
        }
    }
}

void FirebaseManager::NotifyListeners(const std::string& path, const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);

    for (auto& [id, listener] : m_listeners) {
        if (listener.active && listener.callback) {
            // Check if the listener path matches or is a parent of the changed path
            if (path.find(listener.path) == 0 || listener.path.find(path) == 0) {
                listener.callback(data);
            }
        }
    }
}

std::string FirebaseManager::GenerateUniqueId() {
    // Generate a unique ID similar to Firebase push IDs
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::stringstream ss;

    // Encode timestamp (8 chars)
    for (int i = 7; i >= 0; --i) {
        ss << chars[static_cast<size_t>(timestamp % 64)];
        timestamp /= 64;
    }

    // Add random suffix (12 chars)
    for (int i = 0; i < 12; ++i) {
        ss << chars[dis(gen)];
    }

    return ss.str();
}

bool FirebaseManager::LoadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        // Use default stub config
        m_config.projectId = "vehement2-stub";
        m_config.apiKey = "stub-api-key";
        m_config.databaseUrl = "https://vehement2-stub.firebaseio.com";
        FIREBASE_LOG_WARN("Config file not found, using stub configuration");
        return true;
    }

    try {
        nlohmann::json configJson;
        file >> configJson;

        m_config.projectId = configJson.value("projectId", "");
        m_config.apiKey = configJson.value("apiKey", "");
        m_config.authDomain = configJson.value("authDomain", "");
        m_config.databaseUrl = configJson.value("databaseUrl", "");
        m_config.storageBucket = configJson.value("storageBucket", "");
        m_config.messagingSenderId = configJson.value("messagingSenderId", "");
        m_config.appId = configJson.value("appId", "");

        return true;
    } catch (const std::exception& e) {
        FIREBASE_LOG_ERROR("Failed to parse config: " + std::string(e.what()));
        return false;
    }
}

void FirebaseManager::SimulateNetworkDelay(std::function<void()> operation) {
    // In stub mode, execute immediately
    // In real implementation, this would be async
    if (operation) {
        operation();
    }
}

} // namespace Vehement
