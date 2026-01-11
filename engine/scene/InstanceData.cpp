#include "InstanceData.hpp"
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

namespace Nova {

nlohmann::json InstanceData::ToJson() const {
    nlohmann::json json;

    // Basic identification
    json["archetype"] = archetypeId;
    json["instanceId"] = instanceId;

    // Transform
    json["transform"] = {
        {"position", {position.x, position.y, position.z}},
        {"rotation", {rotation.w, rotation.x, rotation.y, rotation.z}},  // w, x, y, z order
        {"scale", {scale.x, scale.y, scale.z}}
    };

    // Property overrides
    if (!overrides.empty()) {
        json["overrides"] = overrides;
    }

    // Custom data
    if (!customData.empty()) {
        json["customData"] = customData;
    }

    // Metadata
    if (!name.empty()) {
        json["name"] = name;
    }

    return json;
}

InstanceData InstanceData::FromJson(const nlohmann::json& json) {
    InstanceData data;

    // Basic identification
    if (json.contains("archetype")) {
        data.archetypeId = json["archetype"].get<std::string>();
    }

    if (json.contains("instanceId")) {
        data.instanceId = json["instanceId"].get<std::string>();
    } else {
        data.instanceId = GenerateInstanceId();
    }

    // Transform
    if (json.contains("transform")) {
        const auto& transform = json["transform"];

        if (transform.contains("position") && transform["position"].is_array()) {
            auto pos = transform["position"];
            if (pos.size() >= 3) {
                data.position = glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
            }
        }

        if (transform.contains("rotation") && transform["rotation"].is_array()) {
            auto rot = transform["rotation"];
            if (rot.size() >= 4) {
                // w, x, y, z order
                data.rotation = glm::quat(rot[0].get<float>(), rot[1].get<float>(),
                                          rot[2].get<float>(), rot[3].get<float>());
            }
        }

        if (transform.contains("scale") && transform["scale"].is_array()) {
            auto scl = transform["scale"];
            if (scl.size() >= 3) {
                data.scale = glm::vec3(scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>());
            }
        }
    }

    // Property overrides
    if (json.contains("overrides")) {
        data.overrides = json["overrides"];
    }

    // Custom data
    if (json.contains("customData")) {
        data.customData = json["customData"];
    }

    // Metadata
    if (json.contains("name")) {
        data.name = json["name"].get<std::string>();
    }

    data.isDirty = false;
    return data;
}

InstanceData InstanceData::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open instance file: {}", path);
            return InstanceData();
        }

        nlohmann::json json;
        file >> json;
        file.close();

        return FromJson(json);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load instance from file {}: {}", path, e.what());
        return InstanceData();
    }
}

bool InstanceData::SaveToFile(const std::string& path) const {
    try {
        nlohmann::json json = ToJson();

        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", path);
            return false;
        }

        file << json.dump(2);  // Pretty print with 2-space indentation
        file.close();

        spdlog::info("Saved instance to: {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save instance to file {}: {}", path, e.what());
        return false;
    }
}

std::string InstanceData::GenerateInstanceId() {
    // Generate a simple UUID-like identifier
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (part1 >> 32);
    ss << "-";
    ss << std::setw(4) << ((part1 >> 16) & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part1 & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part2 >> 48);
    ss << "-";
    ss << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);

    return ss.str();
}

bool InstanceData::HasOverride(const std::string& propertyPath) const {
    try {
        // Navigate nested path
        nlohmann::json current = overrides;
        size_t start = 0;
        size_t end = propertyPath.find('.');

        while (end != std::string::npos) {
            std::string key = propertyPath.substr(start, end - start);
            if (!current.contains(key)) {
                return false;
            }
            current = current[key];
            start = end + 1;
            end = propertyPath.find('.', start);
        }

        std::string finalKey = propertyPath.substr(start);
        return current.contains(finalKey);
    } catch (...) {
        return false;
    }
}

void InstanceData::RemoveOverride(const std::string& propertyPath) {
    try {
        // Navigate to parent and remove the final key
        nlohmann::json* current = &overrides;
        size_t start = 0;
        size_t end = propertyPath.find('.');
        std::vector<std::string> path;

        while (end != std::string::npos) {
            std::string key = propertyPath.substr(start, end - start);
            path.push_back(key);
            start = end + 1;
            end = propertyPath.find('.', start);
        }

        std::string finalKey = propertyPath.substr(start);

        // Navigate to parent
        for (const auto& key : path) {
            if (!current->contains(key)) {
                return;  // Path doesn't exist
            }
            current = &(*current)[key];
        }

        // Remove final key
        if (current->contains(finalKey)) {
            current->erase(finalKey);
            isDirty = true;
        }
    } catch (...) {
        // Ignore errors
    }
}

} // namespace Nova
