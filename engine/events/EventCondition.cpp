#include "EventCondition.hpp"
#ifdef NOVA_SCRIPTING_ENABLED
#include "../scripting/PythonEngine.hpp"
#endif
#include <sstream>
#include <algorithm>
#include <cmath>
#include <regex>

namespace Nova {
namespace Events {

#ifdef NOVA_SCRIPTING_ENABLED
// Static member initialization
Nova::Scripting::PythonEngine* EventConditionEvaluator::s_pythonEngine = nullptr;
#endif

// ============================================================================
// Comparator Utilities
// ============================================================================

const char* ComparatorToString(Comparator comp) {
    switch (comp) {
        case Comparator::Equal: return "==";
        case Comparator::NotEqual: return "!=";
        case Comparator::LessThan: return "<";
        case Comparator::LessOrEqual: return "<=";
        case Comparator::GreaterThan: return ">";
        case Comparator::GreaterOrEqual: return ">=";
        case Comparator::Changed: return "changed";
        case Comparator::Contains: return "contains";
        case Comparator::StartsWith: return "startsWith";
        case Comparator::EndsWith: return "endsWith";
        case Comparator::Matches: return "matches";
        case Comparator::InRange: return "inRange";
        case Comparator::IsNull: return "isNull";
        case Comparator::IsNotNull: return "isNotNull";
        case Comparator::BitSet: return "bitSet";
        case Comparator::BitClear: return "bitClear";
        default: return "unknown";
    }
}

std::optional<Comparator> ComparatorFromString(const std::string& str) {
    if (str == "==" || str == "eq" || str == "equal") return Comparator::Equal;
    if (str == "!=" || str == "ne" || str == "notEqual") return Comparator::NotEqual;
    if (str == "<" || str == "lt" || str == "lessThan") return Comparator::LessThan;
    if (str == "<=" || str == "le" || str == "lessOrEqual") return Comparator::LessOrEqual;
    if (str == ">" || str == "gt" || str == "greaterThan") return Comparator::GreaterThan;
    if (str == ">=" || str == "ge" || str == "greaterOrEqual") return Comparator::GreaterOrEqual;
    if (str == "changed") return Comparator::Changed;
    if (str == "contains") return Comparator::Contains;
    if (str == "startsWith") return Comparator::StartsWith;
    if (str == "endsWith") return Comparator::EndsWith;
    if (str == "matches") return Comparator::Matches;
    if (str == "inRange") return Comparator::InRange;
    if (str == "isNull") return Comparator::IsNull;
    if (str == "isNotNull") return Comparator::IsNotNull;
    if (str == "bitSet") return Comparator::BitSet;
    if (str == "bitClear") return Comparator::BitClear;
    return std::nullopt;
}

// ============================================================================
// EventCondition Methods
// ============================================================================

std::string EventCondition::ToString() const {
    std::ostringstream ss;

    if (!description.empty()) {
        return description;
    }

    ss << "When ";
    if (sourceType != "*") {
        ss << sourceType << " ";
    }

    ss << "emits " << eventName;

    if (!propertyPath.empty()) {
        ss << " and " << propertyPath << " " << ComparatorToString(comparator);

        std::visit([&ss](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                ss << " null";
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << " \"" << val << "\"";
            } else if constexpr (std::is_same_v<T, bool>) {
                ss << " " << (val ? "true" : "false");
            } else if constexpr (std::is_arithmetic_v<T>) {
                ss << " " << val;
            }
        }, compareValue);
    }

    if (!pythonCondition.empty()) {
        ss << " and Python(" << pythonCondition << ")";
    }

    if (negate) {
        return "NOT (" + ss.str() + ")";
    }

    return ss.str();
}

json EventCondition::ToJson() const {
    json j;

    j["sourceType"] = sourceType;
    if (!sourceId.empty()) j["sourceId"] = sourceId;
    j["eventName"] = eventName;
    if (!eventTags.empty()) j["eventTags"] = eventTags;

    if (!propertyPath.empty()) {
        j["propertyPath"] = propertyPath;
        j["comparator"] = ComparatorToString(comparator);

        // Serialize compare value
        std::visit([&j](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                j["compareValue"] = nullptr;
            } else {
                j["compareValue"] = val;
            }
        }, compareValue);
    }

    if (comparator == Comparator::InRange) {
        std::visit([&j](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                j["rangeMin"] = val;
            }
        }, rangeMin);
        std::visit([&j](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                j["rangeMax"] = val;
            }
        }, rangeMax);
    }

    if (!pythonCondition.empty()) j["pythonCondition"] = pythonCondition;
    if (!pythonModule.empty()) j["pythonModule"] = pythonModule;
    if (!pythonFunction.empty()) j["pythonFunction"] = pythonFunction;

    if (negate) j["negate"] = true;
    if (!description.empty()) j["description"] = description;
    if (!id.empty()) j["id"] = id;

    // Compound conditions
    if (!andConditions.empty()) {
        json andArray = json::array();
        for (const auto& cond : andConditions) {
            andArray.push_back(cond->ToJson());
        }
        j["and"] = andArray;
    }

    if (!orConditions.empty()) {
        json orArray = json::array();
        for (const auto& cond : orConditions) {
            orArray.push_back(cond->ToJson());
        }
        j["or"] = orArray;
    }

    return j;
}

EventCondition EventCondition::FromJson(const json& j) {
    EventCondition cond;

    if (j.contains("sourceType")) cond.sourceType = j["sourceType"].get<std::string>();
    if (j.contains("sourceId")) cond.sourceId = j["sourceId"].get<std::string>();
    if (j.contains("eventName")) cond.eventName = j["eventName"].get<std::string>();
    if (j.contains("eventTags")) cond.eventTags = j["eventTags"].get<std::vector<std::string>>();

    if (j.contains("propertyPath")) cond.propertyPath = j["propertyPath"].get<std::string>();
    if (j.contains("comparator")) {
        auto comp = ComparatorFromString(j["comparator"].get<std::string>());
        if (comp) cond.comparator = *comp;
    }

    if (j.contains("compareValue")) {
        const auto& val = j["compareValue"];
        if (val.is_null()) {
            cond.compareValue = std::monostate{};
        } else if (val.is_boolean()) {
            cond.compareValue = val.get<bool>();
        } else if (val.is_number_integer()) {
            cond.compareValue = val.get<int64_t>();
        } else if (val.is_number_float()) {
            cond.compareValue = val.get<double>();
        } else if (val.is_string()) {
            cond.compareValue = val.get<std::string>();
        }
    }

    if (j.contains("rangeMin")) {
        const auto& val = j["rangeMin"];
        if (val.is_number()) cond.rangeMin = val.get<double>();
    }
    if (j.contains("rangeMax")) {
        const auto& val = j["rangeMax"];
        if (val.is_number()) cond.rangeMax = val.get<double>();
    }

    if (j.contains("pythonCondition")) cond.pythonCondition = j["pythonCondition"].get<std::string>();
    if (j.contains("pythonModule")) cond.pythonModule = j["pythonModule"].get<std::string>();
    if (j.contains("pythonFunction")) cond.pythonFunction = j["pythonFunction"].get<std::string>();

    if (j.contains("negate")) cond.negate = j["negate"].get<bool>();
    if (j.contains("description")) cond.description = j["description"].get<std::string>();
    if (j.contains("id")) cond.id = j["id"].get<std::string>();

    // Compound conditions
    if (j.contains("and")) {
        for (const auto& andCond : j["and"]) {
            cond.andConditions.push_back(std::make_shared<EventCondition>(FromJson(andCond)));
        }
    }

    if (j.contains("or")) {
        for (const auto& orCond : j["or"]) {
            cond.orConditions.push_back(std::make_shared<EventCondition>(FromJson(orCond)));
        }
    }

    return cond;
}

// ============================================================================
// EventConditionEvaluator
// ============================================================================

#ifdef NOVA_SCRIPTING_ENABLED
void EventConditionEvaluator::SetPythonEngine(Nova::Scripting::PythonEngine* engine) {
    s_pythonEngine = engine;
}
#endif

bool EventConditionEvaluator::Evaluate(
    const EventCondition& condition,
    const std::string& eventType,
    const std::string& sourceType,
    const std::string& sourceId,
    const void* source,
    const Reflect::TypeInfo* typeInfo,
    const std::unordered_map<std::string, std::any>* eventData
) {
    bool result = true;

    // Check source type filter
    if (condition.sourceType != "*" && condition.sourceType != sourceType) {
        result = false;
    }

    // Check source ID filter
    if (result && !condition.sourceId.empty() && condition.sourceId != sourceId) {
        result = false;
    }

    // Check event name
    if (result && !condition.eventName.empty() && condition.eventName != eventType) {
        result = false;
    }

    // Check property condition
    if (result && condition.HasPropertyCondition() && source && typeInfo) {
        auto propValue = GetPropertyValue(source, typeInfo, condition.propertyPath);
        if (propValue) {
            result = CompareValues(
                *propValue,
                condition.comparator,
                condition.compareValue,
                condition.comparator == Comparator::InRange ? &condition.rangeMin : nullptr,
                condition.comparator == Comparator::InRange ? &condition.rangeMax : nullptr
            );
        } else {
            // Property not found
            result = (condition.comparator == Comparator::IsNull);
        }
    }

    // Check Python condition
#ifdef NOVA_SCRIPTING_ENABLED
    if (result && condition.UsesPython() && s_pythonEngine) {
        if (!condition.pythonFunction.empty()) {
            auto pyResult = s_pythonEngine->CallFunction(
                condition.pythonModule,
                condition.pythonFunction,
                eventType, sourceType, sourceId
            );
            if (pyResult.success) {
                // Try to extract boolean result from return value
                result = pyResult.returnValue.index() == 0 ?
                    std::get<bool>(pyResult.returnValue) : false;
            }
        } else if (!condition.pythonCondition.empty()) {
            auto pyResult = s_pythonEngine->ExecuteString(
                "result = " + condition.pythonCondition,
                "condition_eval"
            );
            // Would need to extract 'result' from globals
            // For now, just check if execution succeeded
            result = pyResult.success;
        }
    }
#endif

    // Evaluate AND conditions
    if (result && !condition.andConditions.empty()) {
        for (const auto& andCond : condition.andConditions) {
            if (!Evaluate(*andCond, eventType, sourceType, sourceId, source, typeInfo, eventData)) {
                result = false;
                break;
            }
        }
    }

    // Evaluate OR conditions (if any OR condition passes, the condition passes)
    if (!result && !condition.orConditions.empty()) {
        for (const auto& orCond : condition.orConditions) {
            if (Evaluate(*orCond, eventType, sourceType, sourceId, source, typeInfo, eventData)) {
                result = true;
                break;
            }
        }
    }

    // Apply negation
    return condition.negate ? !result : result;
}

bool EventConditionEvaluator::CompareValues(
    const ConditionValue& actual,
    Comparator comparator,
    const ConditionValue& expected,
    const ConditionValue* rangeMin,
    const ConditionValue* rangeMax
) {
    // Handle null comparisons
    if (comparator == Comparator::IsNull) {
        return std::holds_alternative<std::monostate>(actual);
    }
    if (comparator == Comparator::IsNotNull) {
        return !std::holds_alternative<std::monostate>(actual);
    }

    // Handle Changed comparator (always true if we got here, actual change detection is external)
    if (comparator == Comparator::Changed) {
        return true;
    }

    // Convert values to doubles for numeric comparisons
    auto toDouble = [](const ConditionValue& v) -> std::optional<double> {
        return std::visit([](auto&& val) -> std::optional<double> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, bool>) {
                return val ? 1.0 : 0.0;
            } else if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<double>(val);
            }
            return std::nullopt;
        }, v);
    };

    auto toString = [](const ConditionValue& v) -> std::optional<std::string> {
        return std::visit([](auto&& val) -> std::optional<std::string> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return val;
            }
            return std::nullopt;
        }, v);
    };

    // Numeric comparisons
    auto actualNum = toDouble(actual);
    auto expectedNum = toDouble(expected);

    if (actualNum && expectedNum) {
        switch (comparator) {
            case Comparator::Equal:
                return std::abs(*actualNum - *expectedNum) < 1e-9;
            case Comparator::NotEqual:
                return std::abs(*actualNum - *expectedNum) >= 1e-9;
            case Comparator::LessThan:
                return *actualNum < *expectedNum;
            case Comparator::LessOrEqual:
                return *actualNum <= *expectedNum;
            case Comparator::GreaterThan:
                return *actualNum > *expectedNum;
            case Comparator::GreaterOrEqual:
                return *actualNum >= *expectedNum;
            case Comparator::InRange:
                if (rangeMin && rangeMax) {
                    auto minVal = toDouble(*rangeMin);
                    auto maxVal = toDouble(*rangeMax);
                    if (minVal && maxVal) {
                        return *actualNum >= *minVal && *actualNum <= *maxVal;
                    }
                }
                return false;
            case Comparator::BitSet:
                return (static_cast<int64_t>(*actualNum) & static_cast<int64_t>(*expectedNum)) != 0;
            case Comparator::BitClear:
                return (static_cast<int64_t>(*actualNum) & static_cast<int64_t>(*expectedNum)) == 0;
            default:
                break;
        }
    }

    // String comparisons
    auto actualStr = toString(actual);
    auto expectedStr = toString(expected);

    if (actualStr && expectedStr) {
        switch (comparator) {
            case Comparator::Equal:
                return *actualStr == *expectedStr;
            case Comparator::NotEqual:
                return *actualStr != *expectedStr;
            case Comparator::Contains:
                return actualStr->find(*expectedStr) != std::string::npos;
            case Comparator::StartsWith:
                return actualStr->rfind(*expectedStr, 0) == 0;
            case Comparator::EndsWith:
                if (expectedStr->size() > actualStr->size()) return false;
                return actualStr->compare(actualStr->size() - expectedStr->size(),
                                         expectedStr->size(), *expectedStr) == 0;
            case Comparator::Matches:
                try {
                    std::regex re(*expectedStr);
                    return std::regex_match(*actualStr, re);
                } catch (...) {
                    return false;
                }
            default:
                break;
        }
    }

    return false;
}

std::optional<ConditionValue> EventConditionEvaluator::GetPropertyValue(
    const void* object,
    const Reflect::TypeInfo* typeInfo,
    const std::string& propertyPath
) {
    if (!object || !typeInfo) return std::nullopt;

    // Split property path by '.'
    std::vector<std::string> parts;
    std::istringstream ss(propertyPath);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.empty()) return std::nullopt;

    // Navigate through nested properties
    const void* current = object;
    const Reflect::TypeInfo* currentType = typeInfo;

    for (size_t i = 0; i < parts.size(); ++i) {
        const auto* prop = currentType->FindProperty(parts[i]);
        if (!prop) return std::nullopt;

        if (prop->getterAny) {
            std::any value = prop->getterAny(current);

            // If this is the last part, return the value
            if (i == parts.size() - 1) {
                // Convert std::any to ConditionValue
                if (auto* v = std::any_cast<bool>(&value)) return *v;
                if (auto* v = std::any_cast<int>(&value)) return *v;
                if (auto* v = std::any_cast<int64_t>(&value)) return *v;
                if (auto* v = std::any_cast<float>(&value)) return static_cast<double>(*v);
                if (auto* v = std::any_cast<double>(&value)) return *v;
                if (auto* v = std::any_cast<std::string>(&value)) return *v;
                return std::monostate{};
            }

            // Otherwise, need to get the nested object pointer
            // This would require more sophisticated type handling
            return std::nullopt;
        }
    }

    return std::nullopt;
}

// ============================================================================
// ConditionFactory
// ============================================================================

EventCondition ConditionFactory::OnEvent(const std::string& eventName) {
    return EventCondition(eventName);
}

EventCondition ConditionFactory::OnTypedEvent(const std::string& sourceType, const std::string& eventName) {
    return EventCondition(sourceType, eventName);
}

EventCondition ConditionFactory::PropertyBelow(const std::string& propertyPath, double threshold) {
    EventCondition cond;
    cond.propertyPath = propertyPath;
    cond.comparator = Comparator::LessThan;
    cond.compareValue = threshold;
    return cond;
}

EventCondition ConditionFactory::PropertyAbove(const std::string& propertyPath, double threshold) {
    EventCondition cond;
    cond.propertyPath = propertyPath;
    cond.comparator = Comparator::GreaterThan;
    cond.compareValue = threshold;
    return cond;
}

EventCondition ConditionFactory::PropertyEquals(const std::string& propertyPath, ConditionValue value) {
    EventCondition cond;
    cond.propertyPath = propertyPath;
    cond.comparator = Comparator::Equal;
    cond.compareValue = std::move(value);
    return cond;
}

EventCondition ConditionFactory::PropertyChanged(const std::string& propertyPath) {
    EventCondition cond;
    cond.propertyPath = propertyPath;
    cond.comparator = Comparator::Changed;
    return cond;
}

EventCondition ConditionFactory::HealthBelow(double percentage) {
    return PropertyBelow("health.percentage", percentage)
        .WithDescription("Health below " + std::to_string(static_cast<int>(percentage)) + "%");
}

EventCondition ConditionFactory::HealthZero() {
    return PropertyEquals("health.current", 0)
        .WithDescription("Health is zero");
}

EventCondition ConditionFactory::AllOf(std::vector<EventCondition> conditions) {
    if (conditions.empty()) return EventCondition();

    EventCondition result = std::move(conditions[0]);
    for (size_t i = 1; i < conditions.size(); ++i) {
        result.andConditions.push_back(std::make_shared<EventCondition>(std::move(conditions[i])));
    }
    return result;
}

EventCondition ConditionFactory::AnyOf(std::vector<EventCondition> conditions) {
    if (conditions.empty()) return EventCondition();

    EventCondition result = std::move(conditions[0]);
    for (size_t i = 1; i < conditions.size(); ++i) {
        result.orConditions.push_back(std::make_shared<EventCondition>(std::move(conditions[i])));
    }
    return result;
}

} // namespace Events
} // namespace Nova
