#pragma once

#include "../reflection/TypeInfo.hpp"
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <any>
#include <variant>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Nova {
namespace Events {

/**
 * @brief Comparison operators for event conditions
 */
enum class Comparator {
    Equal,          // ==
    NotEqual,       // !=
    LessThan,       // <
    LessOrEqual,    // <=
    GreaterThan,    // >
    GreaterOrEqual, // >=
    Changed,        // Property changed (any change)
    Contains,       // For strings/arrays
    StartsWith,     // String starts with
    EndsWith,       // String ends with
    Matches,        // Regex match
    InRange,        // Value in range [min, max]
    IsNull,         // Value is null/empty
    IsNotNull,      // Value is not null/empty
    BitSet,         // Bitwise flag is set
    BitClear        // Bitwise flag is clear
};

/**
 * @brief Convert comparator to string
 */
[[nodiscard]] const char* ComparatorToString(Comparator comp);

/**
 * @brief Parse comparator from string
 */
[[nodiscard]] std::optional<Comparator> ComparatorFromString(const std::string& str);

/**
 * @brief Condition value variant type
 */
using ConditionValue = std::variant<
    std::monostate,  // null
    bool,
    int,
    int64_t,
    float,
    double,
    std::string,
    std::vector<int>,
    std::vector<float>,
    std::vector<std::string>
>;

/**
 * @brief Condition for filtering which events trigger a binding
 *
 * Allows filtering events based on:
 * - Source type (e.g., "Unit", "Building", "*" for all)
 * - Event name (e.g., "OnDamage", "OnCreate")
 * - Property path and comparison
 * - Custom Python expression
 */
struct EventCondition {
    // Source filter
    std::string sourceType = "*";       // "Unit", "Building", "*"
    std::string sourceId;               // Specific source ID (empty = any)

    // Event filter
    std::string eventName;              // "OnDamage", "OnCreate", etc.
    std::vector<std::string> eventTags; // Additional tags to match

    // Property condition
    std::string propertyPath;           // "health.current", "position.x"
    Comparator comparator = Comparator::Equal;
    ConditionValue compareValue;        // Value to compare against

    // Range comparison (for InRange comparator)
    ConditionValue rangeMin;
    ConditionValue rangeMax;

    // Python condition (optional additional condition)
    std::string pythonCondition;        // Python expression returning bool
    std::string pythonModule;           // Module for condition function
    std::string pythonFunction;         // Function name

    // Negation
    bool negate = false;                // Negate the entire condition

    // Logical operators for compound conditions
    std::vector<std::shared_ptr<EventCondition>> andConditions;
    std::vector<std::shared_ptr<EventCondition>> orConditions;

    // Metadata
    std::string description;
    std::string id;

    // =========================================================================
    // Constructors
    // =========================================================================

    EventCondition() = default;

    EventCondition(const std::string& evtName)
        : eventName(evtName) {}

    EventCondition(const std::string& srcType, const std::string& evtName)
        : sourceType(srcType), eventName(evtName) {}

    EventCondition(const std::string& srcType, const std::string& evtName,
                   const std::string& propPath, Comparator comp, ConditionValue value)
        : sourceType(srcType), eventName(evtName),
          propertyPath(propPath), comparator(comp), compareValue(std::move(value)) {}

    // =========================================================================
    // Builder Pattern
    // =========================================================================

    EventCondition& WithSourceType(const std::string& type) {
        sourceType = type;
        return *this;
    }

    EventCondition& WithSourceId(const std::string& id) {
        sourceId = id;
        return *this;
    }

    EventCondition& WithEventName(const std::string& name) {
        eventName = name;
        return *this;
    }

    EventCondition& WithEventTag(const std::string& tag) {
        eventTags.push_back(tag);
        return *this;
    }

    EventCondition& WithProperty(const std::string& path) {
        propertyPath = path;
        return *this;
    }

    EventCondition& WithComparator(Comparator comp) {
        comparator = comp;
        return *this;
    }

    EventCondition& WithValue(ConditionValue value) {
        compareValue = std::move(value);
        return *this;
    }

    EventCondition& WithRange(ConditionValue min, ConditionValue max) {
        comparator = Comparator::InRange;
        rangeMin = std::move(min);
        rangeMax = std::move(max);
        return *this;
    }

    EventCondition& WithPythonCondition(const std::string& expr) {
        pythonCondition = expr;
        return *this;
    }

    EventCondition& WithPythonFunction(const std::string& module, const std::string& func) {
        pythonModule = module;
        pythonFunction = func;
        return *this;
    }

    EventCondition& Negate() {
        negate = !negate;
        return *this;
    }

    EventCondition& And(std::shared_ptr<EventCondition> condition) {
        andConditions.push_back(std::move(condition));
        return *this;
    }

    EventCondition& Or(std::shared_ptr<EventCondition> condition) {
        orConditions.push_back(std::move(condition));
        return *this;
    }

    EventCondition& WithDescription(const std::string& desc) {
        description = desc;
        return *this;
    }

    EventCondition& WithId(const std::string& condId) {
        id = condId;
        return *this;
    }

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Check if this condition has property constraints
     */
    [[nodiscard]] bool HasPropertyCondition() const {
        return !propertyPath.empty();
    }

    /**
     * @brief Check if this condition uses Python
     */
    [[nodiscard]] bool UsesPython() const {
        return !pythonCondition.empty() || !pythonFunction.empty();
    }

    /**
     * @brief Check if this is a compound condition
     */
    [[nodiscard]] bool IsCompound() const {
        return !andConditions.empty() || !orConditions.empty();
    }

    /**
     * @brief Check if condition matches all sources
     */
    [[nodiscard]] bool MatchesAllSources() const {
        return sourceType == "*" && sourceId.empty();
    }

    /**
     * @brief Get a string description of the condition
     */
    [[nodiscard]] std::string ToString() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static EventCondition FromJson(const json& j);
};

/**
 * @brief Evaluator for event conditions
 */
class EventConditionEvaluator {
public:
    /**
     * @brief Evaluate a condition against an event and optional object
     * @param condition The condition to evaluate
     * @param eventType The event type string
     * @param sourceType Source object type
     * @param sourceId Source object ID
     * @param source Pointer to source object (for property access)
     * @param eventData Additional event data
     * @return true if condition is satisfied
     */
    static bool Evaluate(
        const EventCondition& condition,
        const std::string& eventType,
        const std::string& sourceType,
        const std::string& sourceId,
        const void* source = nullptr,
        const Reflect::TypeInfo* typeInfo = nullptr,
        const std::unordered_map<std::string, std::any>* eventData = nullptr
    );

    /**
     * @brief Compare two values using the specified comparator
     */
    static bool CompareValues(
        const ConditionValue& actual,
        Comparator comparator,
        const ConditionValue& expected,
        const ConditionValue* rangeMin = nullptr,
        const ConditionValue* rangeMax = nullptr
    );

    /**
     * @brief Get property value from an object using path notation
     */
    static std::optional<ConditionValue> GetPropertyValue(
        const void* object,
        const Reflect::TypeInfo* typeInfo,
        const std::string& propertyPath
    );

    /**
     * @brief Set Python engine for evaluating Python conditions
     */
    static void SetPythonEngine(class Nova::Scripting::PythonEngine* engine);

private:
    static class Nova::Scripting::PythonEngine* s_pythonEngine;
};

/**
 * @brief Factory for common condition patterns
 */
class ConditionFactory {
public:
    /**
     * @brief Create a simple event type condition
     */
    static EventCondition OnEvent(const std::string& eventName);

    /**
     * @brief Create a typed event condition
     */
    static EventCondition OnTypedEvent(const std::string& sourceType, const std::string& eventName);

    /**
     * @brief Create a property threshold condition
     */
    static EventCondition PropertyBelow(const std::string& propertyPath, double threshold);
    static EventCondition PropertyAbove(const std::string& propertyPath, double threshold);
    static EventCondition PropertyEquals(const std::string& propertyPath, ConditionValue value);
    static EventCondition PropertyChanged(const std::string& propertyPath);

    /**
     * @brief Create a health-based condition
     */
    static EventCondition HealthBelow(double percentage);
    static EventCondition HealthZero();

    /**
     * @brief Create compound conditions
     */
    static EventCondition AllOf(std::vector<EventCondition> conditions);
    static EventCondition AnyOf(std::vector<EventCondition> conditions);
};

} // namespace Events
} // namespace Nova
