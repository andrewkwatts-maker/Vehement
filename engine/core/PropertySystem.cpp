#include "PropertySystem.hpp"
#include <fstream>
#include <stdexcept>

namespace Nova3D {

// Helper functions for JSON serialization
namespace {
    nlohmann::json SerializeAny(const std::any& value, std::type_index type) {
        nlohmann::json json;

        if (type == typeid(bool)) {
            json = std::any_cast<bool>(value);
        } else if (type == typeid(int)) {
            json = std::any_cast<int>(value);
        } else if (type == typeid(float)) {
            json = std::any_cast<float>(value);
        } else if (type == typeid(double)) {
            json = std::any_cast<double>(value);
        } else if (type == typeid(std::string)) {
            json = std::any_cast<std::string>(value);
        } else if (type == typeid(glm::vec2)) {
            auto v = std::any_cast<glm::vec2>(value);
            json.push_back(v.x);
            json.push_back(v.y);
        } else if (type == typeid(glm::vec3)) {
            auto v = std::any_cast<glm::vec3>(value);
            json.push_back(v.x);
            json.push_back(v.y);
            json.push_back(v.z);
        } else if (type == typeid(glm::vec4)) {
            auto v = std::any_cast<glm::vec4>(value);
            json.push_back(v.x);
            json.push_back(v.y);
            json.push_back(v.z);
            json.push_back(v.w);
        } else if (type == typeid(glm::quat)) {
            auto q = std::any_cast<glm::quat>(value);
            json.push_back(q.x);
            json.push_back(q.y);
            json.push_back(q.z);
            json.push_back(q.w);
        } else {
            // Unknown type, store as empty
            json = nlohmann::json::null;
        }

        return json;
    }

    std::any DeserializeAny(const nlohmann::json& json, std::type_index type) {
        if (type == typeid(bool)) {
            return json.get<bool>();
        } else if (type == typeid(int)) {
            return json.get<int>();
        } else if (type == typeid(float)) {
            return json.get<float>();
        } else if (type == typeid(double)) {
            return json.get<double>();
        } else if (type == typeid(std::string)) {
            return json.get<std::string>();
        } else if (type == typeid(glm::vec2)) {
            return glm::vec2(json[0].get<float>(), json[1].get<float>());
        } else if (type == typeid(glm::vec3)) {
            return glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
        } else if (type == typeid(glm::vec4)) {
            return glm::vec4(json[0].get<float>(), json[1].get<float>(),
                           json[2].get<float>(), json[3].get<float>());
        } else if (type == typeid(glm::quat)) {
            return glm::quat(json[3].get<float>(), json[0].get<float>(),
                           json[1].get<float>(), json[2].get<float>());
        }

        return std::any();
    }

    std::string TypeIndexToString(std::type_index type) {
        if (type == typeid(bool)) return "bool";
        if (type == typeid(int)) return "int";
        if (type == typeid(float)) return "float";
        if (type == typeid(double)) return "double";
        if (type == typeid(std::string)) return "string";
        if (type == typeid(glm::vec2)) return "vec2";
        if (type == typeid(glm::vec3)) return "vec3";
        if (type == typeid(glm::vec4)) return "vec4";
        if (type == typeid(glm::quat)) return "quat";
        return "unknown";
    }

    std::type_index StringToTypeIndex(const std::string& str) {
        if (str == "bool") return typeid(bool);
        if (str == "int") return typeid(int);
        if (str == "float") return typeid(float);
        if (str == "double") return typeid(double);
        if (str == "string") return typeid(std::string);
        if (str == "vec2") return typeid(glm::vec2);
        if (str == "vec3") return typeid(glm::vec3);
        if (str == "vec4") return typeid(glm::vec4);
        if (str == "quat") return typeid(glm::quat);
        return typeid(void);
    }
}

// PropertyValue implementation

nlohmann::json PropertyValue::Serialize() const {
    nlohmann::json json;

    // Serialize metadata
    json["name"] = m_metadata.name;
    json["category"] = m_metadata.category;
    json["tooltip"] = m_metadata.tooltip;
    json["overrideLevel"] = static_cast<int>(m_metadata.overrideLevel);
    json["allowOverride"] = m_metadata.allowOverride;
    json["type"] = TypeIndexToString(m_metadata.type);

    // UI hints
    json["minValue"] = m_metadata.minValue;
    json["maxValue"] = m_metadata.maxValue;
    json["isColor"] = m_metadata.isColor;
    json["isAngle"] = m_metadata.isAngle;
    json["isPercentage"] = m_metadata.isPercentage;

    // Serialize value
    if (m_value.has_value()) {
        json["value"] = SerializeAny(m_value, m_metadata.type);
    }

    return json;
}

void PropertyValue::Deserialize(const nlohmann::json& json) {
    // Deserialize metadata
    m_metadata.name = json["name"].get<std::string>();
    m_metadata.category = json.value("category", "");
    m_metadata.tooltip = json.value("tooltip", "");
    m_metadata.overrideLevel = static_cast<PropertyLevel>(json["overrideLevel"].get<int>());
    m_metadata.allowOverride = json.value("allowOverride", true);

    std::string typeStr = json["type"].get<std::string>();
    m_metadata.type = StringToTypeIndex(typeStr);

    // UI hints
    m_metadata.minValue = json.value("minValue", 0.0f);
    m_metadata.maxValue = json.value("maxValue", 1.0f);
    m_metadata.isColor = json.value("isColor", false);
    m_metadata.isAngle = json.value("isAngle", false);
    m_metadata.isPercentage = json.value("isPercentage", false);

    // Deserialize value
    if (json.contains("value") && !json["value"].is_null()) {
        m_value = DeserializeAny(json["value"], m_metadata.type);
    }
}

// PropertyContainer implementation

nlohmann::json PropertyContainer::Serialize() const {
    nlohmann::json json;
    json["properties"] = nlohmann::json::array();

    for (const auto& [name, prop] : m_properties) {
        nlohmann::json propJson = prop.Serialize();
        json["properties"].push_back(propJson);
    }

    return json;
}

void PropertyContainer::Deserialize(const nlohmann::json& json) {
    if (!json.contains("properties")) {
        return;
    }

    const nlohmann::json& properties = json["properties"];
    for (const auto& propJson : properties) {
        PropertyValue prop;
        prop.Deserialize(propJson);

        std::string name = propJson["name"].get<std::string>();
        m_properties[name] = std::move(prop);
    }
}

void PropertyContainer::SaveToDatabase(class SQLiteDatabase& db, const std::string& tableName) {
    // TODO: Implement SQLite serialization
    // This would create a table with columns: name, type, value_blob, override_level, etc.
    // For now, we'll use JSON serialization to file
}

void PropertyContainer::LoadFromDatabase(class SQLiteDatabase& db, const std::string& tableName) {
    // TODO: Implement SQLite deserialization
}

// PropertySystem implementation

void PropertySystem::SaveProject(const std::string& filepath) {
    nlohmann::json root;

    // Save global container
    root["global"] = m_globalContainer.Serialize();

    // Save asset containers
    root["assets"] = nlohmann::json::array();
    for (const auto& container : m_assetContainers) {
        root["assets"].push_back(container->Serialize());
    }

    // Save instance containers
    root["instances"] = nlohmann::json::array();
    for (const auto& container : m_instanceContainers) {
        root["instances"].push_back(container->Serialize());
    }

    // Write to file
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }


    file << std::setw(2) << root << std::endl;
    file.close();
}

void PropertySystem::LoadProject(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filepath);
    }


    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
    }

    // Load global container
    if (root.contains("global")) {
        m_globalContainer.Deserialize(root["global"]);
    }

    // Load asset containers
    m_assetContainers.clear();
    if (root.contains("assets")) {
        for (const auto& assetJson : root["assets"]) {
            auto container = std::make_unique<PropertyContainer>(&m_globalContainer);
            container->Deserialize(assetJson);
            m_assetContainers.push_back(std::move(container));
        }
    }

    // Load instance containers
    // Note: We need to reconstruct parent links based on saved data
    m_instanceContainers.clear();
    if (root.contains("instances")) {
        for (const auto& instanceJson : root["instances"]) {
            // For now, link to global container
            // In a real implementation, we'd save parent references
            auto container = std::make_unique<PropertyContainer>(&m_globalContainer);
            container->Deserialize(instanceJson);
            m_instanceContainers.push_back(std::move(container));
        }
    }
}

} // namespace Nova3D
