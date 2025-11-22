#include "EntityConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace Config {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

glm::vec2 ParseVec2(const json& j) {
    if (j.is_array() && j.size() >= 2) {
        return glm::vec2(j[0].get<float>(), j[1].get<float>());
    }
    return glm::vec2(0.0f);
}

glm::vec3 ParseVec3(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return glm::vec3(0.0f);
}

glm::vec4 ParseVec4(const json& j) {
    if (j.is_array() && j.size() >= 4) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                        j[2].get<float>(), j[3].get<float>());
    }
    if (j.is_array() && j.size() >= 3) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                        j[2].get<float>(), 1.0f);
    }
    return glm::vec4(1.0f);
}

json Vec2ToJson(const glm::vec2& v) {
    return json::array({v.x, v.y});
}

json Vec3ToJson(const glm::vec3& v) {
    return json::array({v.x, v.y, v.z});
}

json Vec4ToJson(const glm::vec4& v) {
    return json::array({v.x, v.y, v.z, v.w});
}

CollisionConfig ParseCollisionConfig(const json& j) {
    CollisionConfig config;

    if (j.contains("type")) {
        config.type = StringToCollisionShapeType(j["type"].get<std::string>());
    }

    // Parse shape parameters based on type
    if (j.contains("params")) {
        const auto& params = j["params"];

        switch (config.type) {
            case CollisionShapeType::Box: {
                BoxShapeParams box;
                if (params.contains("halfExtents")) {
                    box.halfExtents = ParseVec3(params["halfExtents"]);
                }
                if (params.contains("offset")) {
                    box.offset = ParseVec3(params["offset"]);
                }
                config.params = box;
                break;
            }
            case CollisionShapeType::Sphere: {
                SphereShapeParams sphere;
                if (params.contains("radius")) {
                    sphere.radius = params["radius"].get<float>();
                }
                if (params.contains("offset")) {
                    sphere.offset = ParseVec3(params["offset"]);
                }
                config.params = sphere;
                break;
            }
            case CollisionShapeType::Capsule: {
                CapsuleShapeParams capsule;
                if (params.contains("radius")) {
                    capsule.radius = params["radius"].get<float>();
                }
                if (params.contains("height")) {
                    capsule.height = params["height"].get<float>();
                }
                if (params.contains("offset")) {
                    capsule.offset = ParseVec3(params["offset"]);
                }
                config.params = capsule;
                break;
            }
            case CollisionShapeType::Mesh: {
                MeshShapeParams mesh;
                if (params.contains("meshPath")) {
                    mesh.meshPath = params["meshPath"].get<std::string>();
                }
                if (params.contains("scale")) {
                    mesh.scale = ParseVec3(params["scale"]);
                }
                if (params.contains("convex")) {
                    mesh.convex = params["convex"].get<bool>();
                }
                config.params = mesh;
                break;
            }
            case CollisionShapeType::Compound: {
                CompoundShapeParams compound;
                if (params.contains("shapes") && params["shapes"].is_array()) {
                    for (const auto& shapeJson : params["shapes"]) {
                        CompoundShapeParams::SubShape subShape;
                        subShape.type = StringToCollisionShapeType(
                            shapeJson.value("type", "box"));

                        if (shapeJson.contains("position")) {
                            subShape.localPosition = ParseVec3(shapeJson["position"]);
                        }
                        if (shapeJson.contains("rotation")) {
                            subShape.localRotation = ParseVec3(shapeJson["rotation"]);
                        }

                        // Parse sub-shape params
                        if (shapeJson.contains("params")) {
                            const auto& subParams = shapeJson["params"];
                            if (subShape.type == CollisionShapeType::Box) {
                                BoxShapeParams box;
                                if (subParams.contains("halfExtents")) {
                                    box.halfExtents = ParseVec3(subParams["halfExtents"]);
                                }
                                subShape.params = box;
                            } else if (subShape.type == CollisionShapeType::Sphere) {
                                SphereShapeParams sphere;
                                if (subParams.contains("radius")) {
                                    sphere.radius = subParams["radius"].get<float>();
                                }
                                subShape.params = sphere;
                            } else if (subShape.type == CollisionShapeType::Capsule) {
                                CapsuleShapeParams capsule;
                                if (subParams.contains("radius")) {
                                    capsule.radius = subParams["radius"].get<float>();
                                }
                                if (subParams.contains("height")) {
                                    capsule.height = subParams["height"].get<float>();
                                }
                                subShape.params = capsule;
                            }
                        }

                        compound.shapes.push_back(subShape);
                    }
                }
                config.params = compound;
                break;
            }
            default:
                break;
        }
    }

    // Physics properties
    if (j.contains("mass")) config.mass = j["mass"].get<float>();
    if (j.contains("friction")) config.friction = j["friction"].get<float>();
    if (j.contains("restitution")) config.restitution = j["restitution"].get<float>();
    if (j.contains("static")) config.isStatic = j["static"].get<bool>();
    if (j.contains("trigger")) config.isTrigger = j["trigger"].get<bool>();
    if (j.contains("collisionGroup")) config.collisionGroup = j["collisionGroup"].get<uint32_t>();
    if (j.contains("collisionMask")) config.collisionMask = j["collisionMask"].get<uint32_t>();

    return config;
}

MaterialConfig ParseMaterialConfig(const json& j) {
    MaterialConfig material;

    if (j.contains("diffuse")) material.diffusePath = j["diffuse"].get<std::string>();
    if (j.contains("normal")) material.normalPath = j["normal"].get<std::string>();
    if (j.contains("specular")) material.specularPath = j["specular"].get<std::string>();
    if (j.contains("emissive")) material.emissivePath = j["emissive"].get<std::string>();

    if (j.contains("baseColor")) material.baseColor = ParseVec4(j["baseColor"]);
    if (j.contains("metallic")) material.metallic = j["metallic"].get<float>();
    if (j.contains("roughness")) material.roughness = j["roughness"].get<float>();
    if (j.contains("emissiveStrength")) material.emissiveStrength = j["emissiveStrength"].get<float>();

    if (j.contains("uvScale")) material.uvScale = ParseVec2(j["uvScale"]);
    if (j.contains("uvOffset")) material.uvOffset = ParseVec2(j["uvOffset"]);

    if (j.contains("transparent")) material.transparent = j["transparent"].get<bool>();
    if (j.contains("doubleSided")) material.doubleSided = j["doubleSided"].get<bool>();

    return material;
}

EventHandler ParseEventHandler(const json& j) {
    EventHandler handler;

    if (j.contains("event")) handler.eventName = j["event"].get<std::string>();
    if (j.contains("script")) handler.scriptPath = j["script"].get<std::string>();
    if (j.contains("function")) handler.functionName = j["function"].get<std::string>();
    if (j.contains("async")) handler.async = j["async"].get<bool>();
    if (j.contains("priority")) handler.priority = j["priority"].get<int>();

    return handler;
}

PropertyValue ParsePropertyValue(const json& j) {
    if (j.is_boolean()) {
        return j.get<bool>();
    } else if (j.is_number_integer()) {
        return j.get<int64_t>();
    } else if (j.is_number_float()) {
        return j.get<double>();
    } else if (j.is_string()) {
        return j.get<std::string>();
    } else if (j.is_array()) {
        if (j.size() == 2 && j[0].is_number()) {
            return glm::vec2(j[0].get<float>(), j[1].get<float>());
        } else if (j.size() == 3 && j[0].is_number()) {
            return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        } else if (j.size() == 4 && j[0].is_number()) {
            return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                           j[2].get<float>(), j[3].get<float>());
        } else if (!j.empty() && j[0].is_string()) {
            std::vector<std::string> arr;
            for (const auto& item : j) {
                arr.push_back(item.get<std::string>());
            }
            return arr;
        } else if (!j.empty() && j[0].is_number()) {
            std::vector<double> arr;
            for (const auto& item : j) {
                arr.push_back(item.get<double>());
            }
            return arr;
        }
    }
    return std::string("");
}

json PropertyValueToJson(const PropertyValue& value) {
    return std::visit([](auto&& arg) -> json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            return arg;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return arg;
        } else if constexpr (std::is_same_v<T, double>) {
            return arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            return Vec2ToJson(arg);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            return Vec3ToJson(arg);
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            return Vec4ToJson(arg);
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            return json(arg);
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            return json(arg);
        }
        return json();
    }, value);
}

} // anonymous namespace

// ============================================================================
// EntityConfig Implementation
// ============================================================================

std::string EntityConfig::StripComments(const std::string& json) {
    std::string result;
    result.reserve(json.size());

    bool inString = false;
    bool inSingleComment = false;
    bool inMultiComment = false;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        char next = (i + 1 < json.size()) ? json[i + 1] : '\0';

        if (inSingleComment) {
            if (c == '\n') {
                inSingleComment = false;
                result += c;
            }
            continue;
        }

        if (inMultiComment) {
            if (c == '*' && next == '/') {
                inMultiComment = false;
                ++i;
            }
            continue;
        }

        if (inString) {
            result += c;
            if (c == '"' && (i == 0 || json[i - 1] != '\\')) {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
            result += c;
            continue;
        }

        if (c == '/' && next == '/') {
            inSingleComment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '*') {
            inMultiComment = true;
            ++i;
            continue;
        }

        result += c;
    }

    return result;
}

bool EntityConfig::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    m_sourcePath = filePath;

    // Get last modified time
    try {
        auto ftime = fs::last_write_time(filePath);
        m_lastModified = ftime.time_since_epoch().count();
    } catch (...) {
        m_lastModified = 0;
    }

    return LoadFromString(buffer.str());
}

bool EntityConfig::LoadFromString(const std::string& jsonString) {
    try {
        // Strip comments for JSON5 support
        std::string cleanJson = StripComments(jsonString);

        json j = json::parse(cleanJson);

        // Parse identity
        if (j.contains("id")) m_id = j["id"].get<std::string>();
        if (j.contains("name")) m_name = j["name"].get<std::string>();
        if (j.contains("description")) m_description = j["description"].get<std::string>();

        // Parse tags
        if (j.contains("tags") && j["tags"].is_array()) {
            m_tags.clear();
            for (const auto& tag : j["tags"]) {
                m_tags.push_back(tag.get<std::string>());
            }
        }

        // Parse inheritance
        if (j.contains("extends")) m_baseConfigId = j["extends"].get<std::string>();

        // Parse model
        if (j.contains("model")) {
            const auto& model = j["model"];
            if (model.is_string()) {
                m_modelPath = model.get<std::string>();
            } else if (model.is_object()) {
                if (model.contains("path")) m_modelPath = model["path"].get<std::string>();
                if (model.contains("scale")) m_modelScale = ParseVec3(model["scale"]);
                if (model.contains("rotation")) m_modelRotation = ParseVec3(model["rotation"]);
                if (model.contains("offset")) m_modelOffset = ParseVec3(model["offset"]);
            }
        }

        // Parse textures
        if (j.contains("texture")) {
            m_texturePath = j["texture"].get<std::string>();
        }
        if (j.contains("textures") && j["textures"].is_object()) {
            for (auto& [key, value] : j["textures"].items()) {
                m_textures[key] = value.get<std::string>();
            }
        }

        // Parse material
        if (j.contains("material")) {
            m_material = ParseMaterialConfig(j["material"]);
        }

        // Parse collision
        if (j.contains("collision")) {
            m_collision = ParseCollisionConfig(j["collision"]);
        }

        // Parse event handlers
        if (j.contains("events") && j["events"].is_object()) {
            m_eventHandlers.clear();
            for (auto& [eventName, handlerJson] : j["events"].items()) {
                EventHandler handler;
                handler.eventName = eventName;

                if (handlerJson.is_string()) {
                    handler.scriptPath = handlerJson.get<std::string>();
                } else if (handlerJson.is_object()) {
                    if (handlerJson.contains("script")) {
                        handler.scriptPath = handlerJson["script"].get<std::string>();
                    }
                    if (handlerJson.contains("function")) {
                        handler.functionName = handlerJson["function"].get<std::string>();
                    }
                    if (handlerJson.contains("async")) {
                        handler.async = handlerJson["async"].get<bool>();
                    }
                    if (handlerJson.contains("priority")) {
                        handler.priority = handlerJson["priority"].get<int>();
                    }
                }

                m_eventHandlers.push_back(handler);
            }
        }

        // Parse custom properties
        if (j.contains("properties") && j["properties"].is_object()) {
            m_properties.Clear();
            for (auto& [key, value] : j["properties"].items()) {
                m_properties.Set(key, ParsePropertyValue(value));
            }
        }

        // Parse type-specific fields
        ParseTypeSpecificFields(cleanJson);

        return true;
    } catch (const json::exception& e) {
        // Log error
        return false;
    }
}

bool EntityConfig::SaveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJsonString();
    return true;
}

std::string EntityConfig::ToJsonString() const {
    json j;

    // Identity
    j["id"] = m_id;
    j["name"] = m_name;
    j["type"] = GetConfigType();

    if (!m_description.empty()) {
        j["description"] = m_description;
    }

    if (!m_tags.empty()) {
        j["tags"] = m_tags;
    }

    // Inheritance
    if (!m_baseConfigId.empty()) {
        j["extends"] = m_baseConfigId;
    }

    // Model
    if (!m_modelPath.empty()) {
        json model;
        model["path"] = m_modelPath;
        if (m_modelScale != glm::vec3(1.0f)) {
            model["scale"] = Vec3ToJson(m_modelScale);
        }
        if (m_modelRotation != glm::vec3(0.0f)) {
            model["rotation"] = Vec3ToJson(m_modelRotation);
        }
        if (m_modelOffset != glm::vec3(0.0f)) {
            model["offset"] = Vec3ToJson(m_modelOffset);
        }
        j["model"] = model;
    }

    // Textures
    if (!m_texturePath.empty()) {
        j["texture"] = m_texturePath;
    }
    if (!m_textures.empty()) {
        j["textures"] = m_textures;
    }

    // Material
    if (!m_material.diffusePath.empty()) {
        json mat;
        mat["diffuse"] = m_material.diffusePath;
        if (!m_material.normalPath.empty()) mat["normal"] = m_material.normalPath;
        if (!m_material.specularPath.empty()) mat["specular"] = m_material.specularPath;
        if (m_material.baseColor != glm::vec4(1.0f)) {
            mat["baseColor"] = Vec4ToJson(m_material.baseColor);
        }
        mat["metallic"] = m_material.metallic;
        mat["roughness"] = m_material.roughness;
        j["material"] = mat;
    }

    // Collision
    if (m_collision.type != CollisionShapeType::None) {
        json collision;
        collision["type"] = CollisionShapeTypeToString(m_collision.type);
        collision["mass"] = m_collision.mass;
        collision["friction"] = m_collision.friction;
        collision["static"] = m_collision.isStatic;
        j["collision"] = collision;
    }

    // Events
    if (!m_eventHandlers.empty()) {
        json events;
        for (const auto& handler : m_eventHandlers) {
            json h;
            h["script"] = handler.scriptPath;
            if (!handler.functionName.empty()) h["function"] = handler.functionName;
            if (handler.async) h["async"] = true;
            if (handler.priority != 0) h["priority"] = handler.priority;
            events[handler.eventName] = h;
        }
        j["events"] = events;
    }

    // Properties
    const auto& props = m_properties.GetAll();
    if (!props.empty()) {
        json properties;
        for (const auto& [key, value] : props) {
            properties[key] = PropertyValueToJson(value);
        }
        j["properties"] = properties;
    }

    return j.dump(2);
}

ValidationResult EntityConfig::Validate() const {
    ValidationResult result;

    // Basic validation
    if (m_id.empty()) {
        result.AddError("id", "Config ID is required");
    }

    if (m_name.empty()) {
        result.AddWarning("name", "Config name is recommended");
    }

    // Validate model path exists if specified
    if (!m_modelPath.empty()) {
        // Check if path looks valid (basic check)
        if (m_modelPath.find("..") != std::string::npos) {
            result.AddWarning("model.path", "Path contains '..' which may cause issues");
        }
    }

    // Validate event handlers
    for (const auto& handler : m_eventHandlers) {
        if (handler.scriptPath.empty()) {
            result.AddError("events." + handler.eventName, "Script path is required");
        }
    }

    return result;
}

bool EntityConfig::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void EntityConfig::ApplyBaseConfig(const EntityConfig& baseConfig) {
    // Copy values from base if not set locally
    if (m_modelPath.empty()) m_modelPath = baseConfig.m_modelPath;
    if (m_modelScale == glm::vec3(1.0f)) m_modelScale = baseConfig.m_modelScale;
    if (m_modelRotation == glm::vec3(0.0f)) m_modelRotation = baseConfig.m_modelRotation;
    if (m_modelOffset == glm::vec3(0.0f)) m_modelOffset = baseConfig.m_modelOffset;

    if (m_texturePath.empty()) m_texturePath = baseConfig.m_texturePath;

    // Merge textures (base first, then override)
    for (const auto& [key, value] : baseConfig.m_textures) {
        if (m_textures.find(key) == m_textures.end()) {
            m_textures[key] = value;
        }
    }

    // Use base material if not set
    if (m_material.diffusePath.empty()) {
        m_material = baseConfig.m_material;
    }

    // Use base collision if not set
    if (m_collision.type == CollisionShapeType::None) {
        m_collision = baseConfig.m_collision;
    }

    // Merge event handlers
    for (const auto& handler : baseConfig.m_eventHandlers) {
        if (!HasEventHandler(handler.eventName)) {
            m_eventHandlers.push_back(handler);
        }
    }

    // Merge properties (base first, then override)
    for (const auto& [key, value] : baseConfig.m_properties.GetAll()) {
        if (!m_properties.Has(key)) {
            m_properties.Set(key, value);
        }
    }

    // Merge tags
    for (const auto& tag : baseConfig.m_tags) {
        if (!HasTag(tag)) {
            m_tags.push_back(tag);
        }
    }
}

std::string EntityConfig::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second : "";
}

std::vector<EventHandler> EntityConfig::GetHandlersForEvent(const std::string& eventName) const {
    std::vector<EventHandler> handlers;
    for (const auto& handler : m_eventHandlers) {
        if (handler.eventName == eventName) {
            handlers.push_back(handler);
        }
    }
    // Sort by priority (higher first)
    std::sort(handlers.begin(), handlers.end(),
              [](const EventHandler& a, const EventHandler& b) {
                  return a.priority > b.priority;
              });
    return handlers;
}

bool EntityConfig::HasEventHandler(const std::string& eventName) const {
    for (const auto& handler : m_eventHandlers) {
        if (handler.eventName == eventName) {
            return true;
        }
    }
    return false;
}

void EntityConfig::ParseTypeSpecificFields(const std::string& /*jsonContent*/) {
    // Base implementation does nothing; derived classes override
}

std::string EntityConfig::SerializeTypeSpecificFields() const {
    // Base implementation returns empty; derived classes override
    return "";
}

// ============================================================================
// EntityConfigFactory Implementation
// ============================================================================

EntityConfigFactory& EntityConfigFactory::Instance() {
    static EntityConfigFactory instance;
    return instance;
}

void EntityConfigFactory::RegisterType(const std::string& typeName, ConfigCreator creator) {
    m_creators[typeName] = std::move(creator);
}

std::unique_ptr<EntityConfig> EntityConfigFactory::Create(const std::string& typeName) const {
    auto it = m_creators.find(typeName);
    if (it != m_creators.end()) {
        return it->second();
    }
    return std::make_unique<EntityConfig>();
}

bool EntityConfigFactory::HasType(const std::string& typeName) const {
    return m_creators.find(typeName) != m_creators.end();
}

std::vector<std::string> EntityConfigFactory::GetRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(m_creators.size());
    for (const auto& [name, _] : m_creators) {
        types.push_back(name);
    }
    return types;
}

} // namespace Config
} // namespace Vehement
