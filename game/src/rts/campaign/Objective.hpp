#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <variant>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Types of mission objectives
 */
enum class ObjectiveType : uint8_t {
    Kill,           ///< Kill specific units or unit types
    Escort,         ///< Protect and escort units to destination
    Capture,        ///< Capture buildings or locations
    Survive,        ///< Survive for a duration
    Collect,        ///< Gather resources or items
    Build,          ///< Construct buildings
    Research,       ///< Research technologies
    Defend,         ///< Defend location from attack
    Destroy,        ///< Destroy buildings or structures
    Reach,          ///< Reach a location with units
    Train,          ///< Train specific units
    Custom          ///< Custom script-driven objective
};

/**
 * @brief Priority/importance of objective
 */
enum class ObjectivePriority : uint8_t {
    Primary,        ///< Must complete to win mission
    Secondary,      ///< Optional but provides rewards
    Bonus,          ///< Hidden or bonus objectives
    Hidden          ///< Not shown until triggered
};

/**
 * @brief Current state of an objective
 */
enum class ObjectiveState : uint8_t {
    Locked,         ///< Not yet available
    Active,         ///< Currently active
    Completed,      ///< Successfully completed
    Failed,         ///< Failed (may fail mission)
    Cancelled       ///< Cancelled (no longer relevant)
};

/**
 * @brief Target specification for objectives
 */
struct ObjectiveTarget {
    std::string targetType;         ///< Unit type, building type, location ID
    std::string targetId;           ///< Specific entity ID (optional)
    int32_t count = 1;              ///< Number required
    float x = 0.0f, y = 0.0f;       ///< Location coordinates
    float radius = 0.0f;            ///< Area radius
    float duration = 0.0f;          ///< Time requirement (seconds)
    std::string resourceType;        ///< Resource type for collect
    int32_t resourceAmount = 0;      ///< Amount required
    std::vector<std::string> tags;  ///< Match units with tags
};

/**
 * @brief Reward granted on objective completion
 */
struct ObjectiveReward {
    int32_t gold = 0;
    int32_t wood = 0;
    int32_t stone = 0;
    int32_t metal = 0;
    int32_t food = 0;
    int32_t experience = 0;
    std::vector<std::string> unlockedUnits;
    std::vector<std::string> unlockedBuildings;
    std::vector<std::string> unlockedAbilities;
    std::vector<std::string> items;
    std::string storyFlag;              ///< Story flag to set on completion
};

/**
 * @brief Progress tracking for objectives
 */
struct ObjectiveProgress {
    int32_t current = 0;                ///< Current progress count
    int32_t required = 1;               ///< Required count
    float timeRemaining = -1.0f;        ///< Time remaining (-1 = no timer)
    float timeElapsed = 0.0f;           ///< Time spent on objective
    std::vector<std::string> completed; ///< IDs of completed sub-targets
    bool timerExpired = false;

    [[nodiscard]] float GetPercentage() const {
        if (required <= 0) return 1.0f;
        return static_cast<float>(current) / static_cast<float>(required);
    }

    [[nodiscard]] bool IsComplete() const {
        return current >= required;
    }
};

/**
 * @brief Hint to help player complete objective
 */
struct ObjectiveHint {
    std::string text;               ///< Hint text
    float showAfterSeconds = 60.0f; ///< When to show hint
    bool shown = false;             ///< Has hint been shown
    std::string icon;               ///< Optional hint icon
};

/**
 * @brief Mission objective definition
 */
class Objective {
public:
    Objective() = default;
    explicit Objective(const std::string& id);
    ~Objective() = default;

    // Identification
    std::string id;
    std::string title;
    std::string description;
    std::string shortDescription;       ///< Brief version for HUD

    // Configuration
    ObjectiveType type = ObjectiveType::Kill;
    ObjectivePriority priority = ObjectivePriority::Primary;
    ObjectiveState state = ObjectiveState::Locked;

    // Target specification
    ObjectiveTarget target;

    // Progress
    ObjectiveProgress progress;

    // Timing
    float timeLimit = -1.0f;            ///< Time limit in seconds (-1 = none)
    bool failOnTimeout = false;         ///< Does timeout fail objective?

    // Dependencies
    std::vector<std::string> prerequisites;  ///< Objectives that must complete first
    std::vector<std::string> blockedBy;      ///< Objectives that block this one

    // Rewards
    ObjectiveReward reward;
    ObjectiveReward bonusReward;        ///< Extra reward for fast completion
    float bonusTimeThreshold = 0.0f;    ///< Complete before this for bonus

    // Hints
    std::vector<ObjectiveHint> hints;

    // UI
    std::string icon;
    std::string soundOnComplete;
    std::string soundOnFail;
    std::string soundOnUpdate;
    bool showNotification = true;
    bool showOnMinimap = true;
    std::string minimapIcon;

    // Callbacks
    std::function<void(Objective&)> onActivate;
    std::function<void(Objective&)> onProgress;
    std::function<void(Objective&)> onComplete;
    std::function<void(Objective&)> onFail;

    // Custom script
    std::string customScript;
    std::string customCondition;

    // Methods
    void Activate();
    void Update(float deltaTime);
    void UpdateProgress(int32_t delta);
    void SetProgress(int32_t value);
    void Complete();
    void Fail();
    void Cancel();
    void Reset();
    void ShowNextHint();

    [[nodiscard]] bool IsActive() const { return state == ObjectiveState::Active; }
    [[nodiscard]] bool IsCompleted() const { return state == ObjectiveState::Completed; }
    [[nodiscard]] bool IsFailed() const { return state == ObjectiveState::Failed; }
    [[nodiscard]] bool IsPrimary() const { return priority == ObjectivePriority::Primary; }
    [[nodiscard]] bool HasTimer() const { return timeLimit > 0.0f; }
    [[nodiscard]] bool CanActivate(const std::vector<Objective>& allObjectives) const;

    // Serialization
    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(const std::string& json);
};

/**
 * @brief Factory for creating objectives from config
 */
class ObjectiveFactory {
public:
    [[nodiscard]] static std::unique_ptr<Objective> Create(ObjectiveType type);
    [[nodiscard]] static std::unique_ptr<Objective> CreateFromJson(const std::string& json);
    [[nodiscard]] static std::unique_ptr<Objective> CreateKill(const std::string& id,
                                                                const std::string& targetType,
                                                                int32_t count);
    [[nodiscard]] static std::unique_ptr<Objective> CreateSurvive(const std::string& id,
                                                                   float duration);
    [[nodiscard]] static std::unique_ptr<Objective> CreateCapture(const std::string& id,
                                                                   const std::string& targetId);
    [[nodiscard]] static std::unique_ptr<Objective> CreateBuild(const std::string& id,
                                                                 const std::string& buildingType,
                                                                 int32_t count);
    [[nodiscard]] static std::unique_ptr<Objective> CreateCollect(const std::string& id,
                                                                   const std::string& resourceType,
                                                                   int32_t amount);
    [[nodiscard]] static std::unique_ptr<Objective> CreateReach(const std::string& id,
                                                                 float x, float y, float radius);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
