#include "SessionManager.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace Vehement2 {
namespace RTS {

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager() = default;

SessionManager::~SessionManager() {
    Shutdown();
}

bool SessionManager::Initialize(SessionFogOfWar* fogOfWar, Exploration* exploration) {
    if (m_initialized) {
        spdlog::warn("SessionManager already initialized");
        return true;
    }

    m_fogOfWar = fogOfWar;
    m_exploration = exploration;
    m_state = SessionState::None;
    m_initialized = true;

    spdlog::info("SessionManager initialized");
    return true;
}

void SessionManager::Shutdown() {
    if (!m_initialized) return;

    // End any active session
    if (m_state == SessionState::Active || m_state == SessionState::Paused) {
        EndSession(SessionEndReason::ServerShutdown);
    }

    m_initialized = false;
    spdlog::info("SessionManager shutdown");
}

void SessionManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Handle different states
    switch (m_state) {
        case SessionState::Active:
            UpdateTimers(deltaTime);
            CheckInactivityTimeout();
            UpdateStatsFromSystems();
            break;

        case SessionState::Paused:
            // Don't update timers when paused
            break;

        case SessionState::Ending:
            // Session is ending, no updates
            m_state = SessionState::None;
            break;

        default:
            break;
    }

    // Check for reconnection timeout
    if (m_waitingForReconnect) {
        CheckReconnectTimeout();
    }
}

// ============================================================================
// Session Lifecycle
// ============================================================================

void SessionManager::StartSession() {
    spdlog::info("Starting new game session");

    // End any existing session
    if (m_state == SessionState::Active || m_state == SessionState::Paused) {
        EndSession(SessionEndReason::NewSession);
    }

    // Reset for new session
    ResetForNewSession();

    // Update state
    m_state = SessionState::Active;
    m_sessionStartTime = std::chrono::steady_clock::now();
    m_lastActivityTime = m_sessionStartTime;
    m_currentStats.startTime = std::chrono::system_clock::now();

    // Reset fog of war if configured
    if (m_config.resetFogOnNewSession && m_fogOfWar) {
        m_fogOfWar->ResetFogOfWar();
    }

    // Notify callback
    if (m_onSessionStart) {
        m_onSessionStart();
    }

    spdlog::info("Game session started");
}

void SessionManager::EndSession(SessionEndReason reason) {
    if (m_state == SessionState::None || m_state == SessionState::Ending) {
        return;
    }

    spdlog::info("Ending game session (reason: {})", static_cast<int>(reason));

    m_state = SessionState::Ending;

    // Finalize stats
    m_currentStats.duration = GetSessionDuration();
    m_currentStats.endTime = std::chrono::system_clock::now();
    UpdateStatsFromSystems();

    // Save stats to history
    if (m_config.saveStatsOnEnd) {
        m_sessionHistory.push_back(m_currentStats);
    }

    // Handle fog of war reset based on reason
    if (m_fogOfWar) {
        bool shouldReset = false;

        switch (reason) {
            case SessionEndReason::PlayerDisconnect:
                shouldReset = m_config.resetFogOnDisconnect;
                m_fogOfWar->OnPlayerDisconnect();
                break;

            case SessionEndReason::InactivityTimeout:
                shouldReset = true;  // Always reset on timeout
                break;

            case SessionEndReason::PlayerDeath:
                shouldReset = m_config.resetFogOnDeath;
                break;

            case SessionEndReason::NewSession:
                shouldReset = m_config.resetFogOnNewSession;
                break;

            default:
                break;
        }

        if (shouldReset && !m_config.saveProgressOnEnd) {
            m_fogOfWar->ResetFogOfWar();
        }
    }

    // Notify callback
    if (m_onSessionEnd) {
        m_onSessionEnd(reason, m_currentStats);
    }

    spdlog::info("Session ended - Duration: {:.1f}s, Score: {:.0f}, Grade: {}",
                 m_currentStats.duration,
                 m_currentStats.CalculateScore(),
                 m_currentStats.GetGrade());
}

void SessionManager::PauseSession() {
    if (m_state != SessionState::Active) {
        spdlog::warn("Cannot pause session: not active");
        return;
    }

    m_state = SessionState::Paused;
    spdlog::info("Session paused");
}

void SessionManager::ResumeSession() {
    if (m_state != SessionState::Paused) {
        spdlog::warn("Cannot resume session: not paused");
        return;
    }

    m_state = SessionState::Active;
    m_lastActivityTime = std::chrono::steady_clock::now();
    m_warningShown = false;

    spdlog::info("Session resumed");
}

// ============================================================================
// Activity Tracking
// ============================================================================

void SessionManager::RecordActivity() {
    if (m_state != SessionState::Active) return;

    m_lastActivityTime = std::chrono::steady_clock::now();
    m_warningShown = false;

    // Also notify fog of war
    if (m_fogOfWar) {
        m_fogOfWar->RecordActivity();
    }
}

bool SessionManager::IsSessionExpired() const {
    if (m_state != SessionState::Active) return false;
    return GetTimeSinceLastActivity() >= m_config.inactivityTimeout;
}

float SessionManager::GetTimeUntilExpiry() const {
    if (m_state != SessionState::Active) return -1.0f;
    float elapsed = GetTimeSinceLastActivity();
    return std::max(0.0f, m_config.inactivityTimeout - elapsed);
}

float SessionManager::GetTimeSinceLastActivity() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - m_lastActivityTime).count();
}

bool SessionManager::ShouldShowInactivityWarning() const {
    if (m_state != SessionState::Active) return false;
    float timeRemaining = GetTimeUntilExpiry();
    return timeRemaining > 0.0f && timeRemaining <= m_config.warningTime;
}

// ============================================================================
// Disconnect/Reconnect
// ============================================================================

void SessionManager::OnPlayerDisconnect() {
    spdlog::info("Player disconnected");

    if (m_config.allowReconnect) {
        m_waitingForReconnect = true;
        m_disconnectTime = std::chrono::steady_clock::now();
        m_reconnectTimer = m_config.reconnectGracePeriod;
        PauseSession();
    } else {
        EndSession(SessionEndReason::PlayerDisconnect);
    }
}

bool SessionManager::OnPlayerReconnect() {
    spdlog::info("Player attempting to reconnect");

    if (m_waitingForReconnect) {
        float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_disconnectTime).count();

        if (elapsed < m_config.reconnectGracePeriod) {
            m_waitingForReconnect = false;
            ResumeSession();

            if (m_fogOfWar) {
                m_fogOfWar->OnPlayerReconnect();
            }

            spdlog::info("Player reconnected successfully");
            return true;
        } else {
            spdlog::warn("Reconnection grace period expired");
            m_waitingForReconnect = false;
            EndSession(SessionEndReason::PlayerDisconnect);
            return false;
        }
    }

    // No active session to reconnect to
    return false;
}

float SessionManager::GetReconnectTimeRemaining() const {
    if (!m_waitingForReconnect) return 0.0f;

    float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - m_disconnectTime).count();
    return std::max(0.0f, m_config.reconnectGracePeriod - elapsed);
}

// ============================================================================
// Statistics
// ============================================================================

float SessionManager::GetSessionDuration() const {
    if (m_state == SessionState::None) return 0.0f;

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - m_sessionStartTime).count();
}

void SessionManager::RecordDeath() {
    m_currentStats.deaths++;

    if (m_config.resetFogOnDeath) {
        EndSession(SessionEndReason::PlayerDeath);
        StartSession();  // Immediately start new session
    }
}

float SessionManager::GetBestScore() const {
    float bestScore = 0.0f;
    for (const auto& stats : m_sessionHistory) {
        bestScore = std::max(bestScore, stats.CalculateScore());
    }
    return bestScore;
}

// ============================================================================
// Internal Methods
// ============================================================================

void SessionManager::UpdateTimers(float deltaTime) {
    // Update autosave timer
    m_autosaveTimer += deltaTime;
    if (m_autosaveTimer >= m_config.autosaveInterval) {
        m_autosaveTimer = 0.0f;
        TriggerAutosave();
    }

    // Update current session duration
    m_currentStats.duration = GetSessionDuration();
}

void SessionManager::CheckInactivityTimeout() {
    float timeRemaining = GetTimeUntilExpiry();

    // Check for warning
    if (ShouldShowInactivityWarning() && !m_warningShown) {
        m_warningShown = true;
        if (m_onInactivityWarning) {
            m_onInactivityWarning(timeRemaining);
        }
        spdlog::warn("Inactivity warning: {} seconds until session expires",
                     static_cast<int>(timeRemaining));
    }

    // Check for timeout
    if (IsSessionExpired()) {
        spdlog::warn("Session expired due to inactivity");
        EndSession(SessionEndReason::InactivityTimeout);
    }
}

void SessionManager::CheckReconnectTimeout() {
    float remaining = GetReconnectTimeRemaining();

    if (remaining <= 0.0f) {
        spdlog::warn("Reconnection grace period expired");
        m_waitingForReconnect = false;
        EndSession(SessionEndReason::PlayerDisconnect);
    }
}

void SessionManager::TriggerAutosave() {
    spdlog::debug("Triggering autosave");
    if (m_onAutosave) {
        m_onAutosave();
    }
}

void SessionManager::UpdateStatsFromSystems() {
    // Update exploration stats from fog of war
    if (m_fogOfWar) {
        m_currentStats.tilesExplored = m_fogOfWar->GetTilesExplored();
        m_currentStats.explorationPercent = m_fogOfWar->GetExplorationPercent();
    }

    // Update discovery stats from exploration system
    if (m_exploration) {
        m_currentStats.discoveriesMade = m_exploration->GetDiscoveryCount();
        m_currentStats.explorationXP = m_exploration->GetTotalExplorationXP();
    }
}

void SessionManager::ResetForNewSession() {
    // Reset stats
    m_currentStats = SessionStats();

    // Reset timers
    m_autosaveTimer = 0.0f;
    m_warningShown = false;
    m_waitingForReconnect = false;
}

} // namespace RTS
} // namespace Vehement2
