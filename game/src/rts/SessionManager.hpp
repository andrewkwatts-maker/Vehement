#pragma once

#include "SessionFogOfWar.hpp"
#include "Exploration.hpp"
#include "VisionSource.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

namespace Vehement2 {
namespace RTS {

// Forward declarations
class SessionFogOfWar;
class Exploration;

/**
 * @brief Session state enumeration
 */
enum class SessionState : uint8_t {
    None,           // No active session
    Starting,       // Session is initializing
    Active,         // Session is running
    Paused,         // Session is paused
    Ending,         // Session is ending
    Expired         // Session has expired
};

/**
 * @brief Reason for session ending
 */
enum class SessionEndReason : uint8_t {
    None,
    PlayerDisconnect,   // Player left the game
    InactivityTimeout,  // Player was idle too long
    NewSession,         // Starting a new session
    PlayerDeath,        // Player died (optional reset)
    ManualEnd,          // Explicit session end request
    ServerShutdown      // Server is shutting down
};

/**
 * @brief Statistics tracked for each session
 */
struct SessionStats {
    // Timing
    float duration = 0.0f;              // Session duration in seconds
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;

    // Exploration
    int tilesExplored = 0;              // Tiles explored this session
    float explorationPercent = 0.0f;    // Percentage of map explored
    int discoveriesMade = 0;            // Number of discoveries found
    float explorationXP = 0.0f;         // XP earned from exploration

    // Combat
    int zombiesKilled = 0;              // Zombies killed this session
    int enemyBasesDestroyed = 0;        // Enemy bases cleared
    float damageDealt = 0.0f;           // Total damage dealt
    float damageTaken = 0.0f;           // Total damage taken
    int deaths = 0;                     // Times player died

    // Economy
    int workersRecruited = 0;           // Workers added to team
    int buildingsConstructed = 0;       // Buildings placed
    int resourcesGathered = 0;          // Total resources collected
    int goldEarned = 0;                 // Currency earned

    // Survivors
    int survivorsRescued = 0;           // NPCs saved

    // Score calculation
    float CalculateScore() const {
        float score = 0.0f;

        // Exploration score (main focus)
        score += explorationPercent * 10.0f;
        score += tilesExplored * 0.1f;
        score += discoveriesMade * 25.0f;

        // Combat score
        score += zombiesKilled * 5.0f;
        score += enemyBasesDestroyed * 100.0f;

        // Economy score
        score += workersRecruited * 20.0f;
        score += buildingsConstructed * 50.0f;
        score += resourcesGathered * 0.5f;

        // Survivor bonus
        score += survivorsRescued * 75.0f;

        // Death penalty
        score -= deaths * 50.0f;

        // Duration bonus (survival time matters)
        score += duration * 0.1f;

        return std::max(0.0f, score);
    }

    /**
     * @brief Get a grade based on score
     */
    std::string GetGrade() const {
        float score = CalculateScore();
        if (score >= 5000.0f) return "S";
        if (score >= 3000.0f) return "A";
        if (score >= 2000.0f) return "B";
        if (score >= 1000.0f) return "C";
        if (score >= 500.0f) return "D";
        return "F";
    }
};

/**
 * @brief Configuration for session management
 */
struct SessionConfig {
    // Timing
    float inactivityTimeout = 30.0f * 60.0f;    // 30 minutes
    float warningTime = 5.0f * 60.0f;           // Warn 5 minutes before timeout
    float autosaveInterval = 60.0f;             // Autosave every minute

    // Reset behavior
    bool resetFogOnDisconnect = true;           // Reset fog when player disconnects
    bool resetFogOnDeath = false;               // Reset fog when player dies
    bool resetFogOnNewSession = true;           // Reset fog when starting new session

    // Persistence
    bool saveStatsOnEnd = true;                 // Save session stats
    bool saveProgressOnEnd = false;             // Save exploration progress (overrides reset)

    // Reconnection
    float reconnectGracePeriod = 5.0f * 60.0f;  // Time to reconnect before reset
    bool allowReconnect = true;                 // Allow reconnection to session
};

/**
 * @brief Callbacks for session events
 */
using SessionStartCallback = std::function<void()>;
using SessionEndCallback = std::function<void(SessionEndReason, const SessionStats&)>;
using SessionWarningCallback = std::function<void(float timeRemaining)>;
using SessionAutosaveCallback = std::function<void()>;

/**
 * @brief Session manager for RTS game sessions
 *
 * Manages the lifecycle of game sessions, including:
 * - Session start/end
 * - Activity tracking and inactivity timeout
 * - Fog of war reset coordination
 * - Session statistics
 * - Autosave functionality
 *
 * The session reset mechanic is key to the RTS design:
 * - Creates tension as fog resets each session
 * - Encourages re-exploration
 * - Prevents stale gameplay
 * - Enables fair multiplayer starts
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize session manager
     * @param fogOfWar Reference to fog of war system
     * @param exploration Reference to exploration system
     * @return true if initialization succeeded
     */
    bool Initialize(SessionFogOfWar* fogOfWar, Exploration* exploration);

    /**
     * @brief Shutdown session manager
     */
    void Shutdown();

    /**
     * @brief Update session manager (call each frame)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Session Lifecycle
    // =========================================================================

    /**
     * @brief Start a new game session
     *
     * This will:
     * - End any existing session
     * - Reset fog of war (if configured)
     * - Reset statistics
     * - Start activity tracking
     */
    void StartSession();

    /**
     * @brief End the current session
     * @param reason Reason for ending
     */
    void EndSession(SessionEndReason reason = SessionEndReason::ManualEnd);

    /**
     * @brief Pause the current session
     *
     * Pausing stops inactivity tracking but keeps the session active.
     */
    void PauseSession();

    /**
     * @brief Resume a paused session
     */
    void ResumeSession();

    /**
     * @brief Get current session state
     */
    SessionState GetState() const { return m_state; }

    /**
     * @brief Check if a session is currently active
     */
    bool IsSessionActive() const { return m_state == SessionState::Active; }

    /**
     * @brief Check if session is paused
     */
    bool IsSessionPaused() const { return m_state == SessionState::Paused; }

    // =========================================================================
    // Activity Tracking
    // =========================================================================

    /**
     * @brief Record player activity (resets inactivity timer)
     *
     * Call this on any player input/action to prevent session timeout.
     */
    void RecordActivity();

    /**
     * @brief Check if session has expired due to inactivity
     */
    bool IsSessionExpired() const;

    /**
     * @brief Get time remaining until session expires
     * @return Seconds until expiry, or -1 if no active session
     */
    float GetTimeUntilExpiry() const;

    /**
     * @brief Get time since last activity
     */
    float GetTimeSinceLastActivity() const;

    /**
     * @brief Check if inactivity warning should be shown
     */
    bool ShouldShowInactivityWarning() const;

    // =========================================================================
    // Disconnect/Reconnect
    // =========================================================================

    /**
     * @brief Handle player disconnect
     *
     * Starts reconnection grace period if configured.
     */
    void OnPlayerDisconnect();

    /**
     * @brief Handle player reconnection
     * @return true if reconnected successfully within grace period
     */
    bool OnPlayerReconnect();

    /**
     * @brief Check if waiting for reconnection
     */
    bool IsWaitingForReconnect() const { return m_waitingForReconnect; }

    /**
     * @brief Get time remaining in reconnect grace period
     */
    float GetReconnectTimeRemaining() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get current session statistics
     */
    const SessionStats& GetCurrentSessionStats() const { return m_currentStats; }

    /**
     * @brief Get mutable reference to stats for updating
     */
    SessionStats& GetMutableStats() { return m_currentStats; }

    /**
     * @brief Get session duration in seconds
     */
    float GetSessionDuration() const;

    /**
     * @brief Record a zombie kill
     */
    void RecordZombieKill() { m_currentStats.zombiesKilled++; }

    /**
     * @brief Record worker recruitment
     */
    void RecordWorkerRecruited() { m_currentStats.workersRecruited++; }

    /**
     * @brief Record building construction
     */
    void RecordBuildingConstructed() { m_currentStats.buildingsConstructed++; }

    /**
     * @brief Record survivor rescue
     */
    void RecordSurvivorRescued() { m_currentStats.survivorsRescued++; }

    /**
     * @brief Record resources gathered
     */
    void RecordResourcesGathered(int amount) { m_currentStats.resourcesGathered += amount; }

    /**
     * @brief Record gold earned
     */
    void RecordGoldEarned(int amount) { m_currentStats.goldEarned += amount; }

    /**
     * @brief Record damage dealt
     */
    void RecordDamageDealt(float damage) { m_currentStats.damageDealt += damage; }

    /**
     * @brief Record damage taken
     */
    void RecordDamageTaken(float damage) { m_currentStats.damageTaken += damage; }

    /**
     * @brief Record player death
     */
    void RecordDeath();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for session start
     */
    void SetSessionStartCallback(SessionStartCallback callback) {
        m_onSessionStart = std::move(callback);
    }

    /**
     * @brief Set callback for session end
     */
    void SetSessionEndCallback(SessionEndCallback callback) {
        m_onSessionEnd = std::move(callback);
    }

    /**
     * @brief Set callback for inactivity warning
     */
    void SetWarningCallback(SessionWarningCallback callback) {
        m_onInactivityWarning = std::move(callback);
    }

    /**
     * @brief Set callback for autosave
     */
    void SetAutosaveCallback(SessionAutosaveCallback callback) {
        m_onAutosave = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void SetConfig(const SessionConfig& config) { m_config = config; }

    /**
     * @brief Get configuration
     */
    const SessionConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Session History
    // =========================================================================

    /**
     * @brief Get history of completed sessions
     */
    const std::vector<SessionStats>& GetSessionHistory() const { return m_sessionHistory; }

    /**
     * @brief Get best session score
     */
    float GetBestScore() const;

    /**
     * @brief Get total sessions played
     */
    int GetTotalSessionsPlayed() const { return static_cast<int>(m_sessionHistory.size()); }

    /**
     * @brief Clear session history
     */
    void ClearHistory() { m_sessionHistory.clear(); }

private:
    // Internal methods
    void UpdateTimers(float deltaTime);
    void CheckInactivityTimeout();
    void CheckReconnectTimeout();
    void TriggerAutosave();
    void UpdateStatsFromSystems();
    void ResetForNewSession();

    // References to game systems
    SessionFogOfWar* m_fogOfWar = nullptr;
    Exploration* m_exploration = nullptr;

    // Configuration
    SessionConfig m_config;

    // State
    SessionState m_state = SessionState::None;
    bool m_initialized = false;

    // Timing
    std::chrono::steady_clock::time_point m_sessionStartTime;
    std::chrono::steady_clock::time_point m_lastActivityTime;
    std::chrono::steady_clock::time_point m_disconnectTime;
    float m_autosaveTimer = 0.0f;

    // Reconnection
    bool m_waitingForReconnect = false;
    float m_reconnectTimer = 0.0f;

    // Warning state
    bool m_warningShown = false;

    // Current session stats
    SessionStats m_currentStats;

    // Session history
    std::vector<SessionStats> m_sessionHistory;

    // Callbacks
    SessionStartCallback m_onSessionStart;
    SessionEndCallback m_onSessionEnd;
    SessionWarningCallback m_onInactivityWarning;
    SessionAutosaveCallback m_onAutosave;
};

} // namespace RTS
} // namespace Vehement2
