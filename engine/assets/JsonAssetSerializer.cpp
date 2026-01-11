#include "JsonAssetSerializer.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <random>
#include <iomanip>
#include <chrono>

namespace Nova {

// =============================================================================
// AssetVersion Implementation
// =============================================================================

AssetVersion AssetVersion::FromString(const std::string& str) {
    AssetVersion version;
    std::sscanf(str.c_str(), "%d.%d.%d", &version.major, &version.minor, &version.patch);
    return version;
}

bool AssetVersion::IsCompatible(const AssetVersion& other) const {
    return major == other.major;
}

// =============================================================================
// AssetMetadata Implementation
// =============================================================================

nlohmann::json AssetMetadata::ToJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type);
    json["version"] = version.ToString();
    json["name"] = name;
    json["uuid"] = uuid;
    json["description"] = description;
    json["tags"] = tags;
    json["dependencies"] = dependencies;
    json["author"] = author;
    json["createdDate"] = createdDate;
    json["modifiedDate"] = modifiedDate;
    json["customProperties"] = customProperties;
    return json;
}

AssetMetadata AssetMetadata::FromJson(const nlohmann::json& json) {
    AssetMetadata metadata;
    if (json.contains("type")) metadata.type = static_cast<AssetType>(json["type"].get<int>());
    if (json.contains("version")) metadata.version = AssetVersion::FromString(json["version"]);
    if (json.contains("name")) metadata.name = json["name"];
    if (json.contains("uuid")) metadata.uuid = json["uuid"];
    if (json.contains("description")) metadata.description = json["description"];
    if (json.contains("tags")) metadata.tags = json["tags"].get<std::vector<std::string>>();
    if (json.contains("dependencies")) metadata.dependencies = json["dependencies"].get<std::vector<std::string>>();
    if (json.contains("author")) metadata.author = json["author"];
    if (json.contains("createdDate")) metadata.createdDate = json["createdDate"];
    if (json.contains("modifiedDate")) metadata.modifiedDate = json["modifiedDate"];
    if (json.contains("customProperties")) {
        metadata.customProperties = json["customProperties"].get<std::unordered_map<std::string, std::string>>();
    }
    return metadata;
}

// =============================================================================
// JsonAsset Implementation
// =============================================================================

bool JsonAsset::IsValid() const {
    return !metadata.uuid.empty() && metadata.type != AssetType::Unknown;
}

// =============================================================================
// JsonAssetSerializer Implementation
// =============================================================================

JsonAssetSerializer::JsonAssetSerializer() {
    // Register default asset types
    RegisterAssetType(AssetType::Material, "material");
    RegisterAssetType(AssetType::Texture, "texture");
    RegisterAssetType(AssetType::Mesh, "mesh");
    RegisterAssetType(AssetType::Model, "model");
    RegisterAssetType(AssetType::Animation, "animation");
    RegisterAssetType(AssetType::Shader, "shader");
    RegisterAssetType(AssetType::Audio, "audio");
    RegisterAssetType(AssetType::Particles, "particles");
    RegisterAssetType(AssetType::Physics, "physics");
    RegisterAssetType(AssetType::VisualScript, "visual_script");
    RegisterAssetType(AssetType::Light, "light");
    RegisterAssetType(AssetType::Prefab, "prefab");
}

JsonAssetSerializer::~JsonAssetSerializer() = default;

std::shared_ptr<JsonAsset> JsonAssetSerializer::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Strip comments
    content = StripComments(content);

    try {
        nlohmann::json json = nlohmann::json::parse(content);
        auto asset = LoadFromJson(json);
        if (asset) {
            asset->sourceFile = filePath;
        }
        return asset;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

std::shared_ptr<JsonAsset> JsonAssetSerializer::LoadFromJson(const nlohmann::json& json) {
    auto asset = std::make_shared<JsonAsset>();

    // Extract metadata
    asset->metadata = AssetMetadata::FromJson(json);

    // If type is in the root, override metadata type
    if (json.contains("type")) {
        asset->metadata.type = GetAssetTypeFromString(json["type"]);
    }

    // Store data
    asset->data = json;

    // Auto-migrate if enabled
    if (m_autoMigrationEnabled) {
        MigrateToLatest(*asset);
    }

    // Validate if enabled
    if (m_validationEnabled) {
        auto result = Validate(*asset);
        if (!result.isValid) {
            // Log errors but still return the asset
        }
    }

    return asset;
}

bool JsonAssetSerializer::SaveToFile(const JsonAsset& asset, const std::string& filePath) {
    nlohmann::json json = SaveToJson(asset);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << json.dump(2); // Pretty print with 2-space indentation
    return true;
}

nlohmann::json JsonAssetSerializer::SaveToJson(const JsonAsset& asset) {
    nlohmann::json json = asset.data;

    // Ensure metadata is included
    json["metadata"] = asset.metadata.ToJson();

    return json;
}

ValidationResult JsonAssetSerializer::Validate(const JsonAsset& asset) const {
    return ValidateJson(asset.data, asset.metadata.type);
}

ValidationResult JsonAssetSerializer::ValidateJson(const nlohmann::json& json, AssetType type) const {
    ValidationResult result;

    // Check if schema exists
    auto it = m_schemas.find(type);
    if (it == m_schemas.end()) {
        result.AddWarning("No schema defined for asset type");
        return result;
    }

    return ValidateAgainstSchema(json, it->second);
}

void JsonAssetSerializer::RegisterAssetType(AssetType type, const std::string& typeName) {
    m_typeNames[type] = typeName;
    m_stringToType[typeName] = type;
}

void JsonAssetSerializer::RegisterSchema(AssetType type, const nlohmann::json& schema) {
    m_schemas[type] = schema;
}

void JsonAssetSerializer::RegisterMigration(AssetType type, std::shared_ptr<IAssetMigration> migration) {
    m_migrations[type].push_back(migration);
}

bool JsonAssetSerializer::MigrateToLatest(JsonAsset& asset) {
    // Find latest version
    // Apply migrations in order
    // For now, just return true
    return true;
}

AssetType JsonAssetSerializer::GetAssetType(const nlohmann::json& json) const {
    if (json.contains("type")) {
        return GetAssetTypeFromString(json["type"]);
    }
    return AssetType::Unknown;
}

AssetType JsonAssetSerializer::GetAssetTypeFromString(const std::string& typeStr) const {
    auto it = m_stringToType.find(typeStr);
    if (it != m_stringToType.end()) {
        return it->second;
    }
    return AssetType::Unknown;
}

std::string JsonAssetSerializer::GetAssetTypeString(AssetType type) const {
    auto it = m_typeNames.find(type);
    if (it != m_typeNames.end()) {
        return it->second;
    }
    return "unknown";
}

std::string JsonAssetSerializer::StripComments(const std::string& jsonWithComments) {
    // Remove single-line comments (// ...)
    std::regex singleLineCommentRegex("//.*");
    std::string temp = std::regex_replace(jsonWithComments, singleLineCommentRegex, "");

    // Remove multi-line comments (/* ... */)
    std::regex multiLineCommentRegex("/\\*[^*]*\\*+(?:[^/*][^*]*\\*+)*/");
    return std::regex_replace(temp, multiLineCommentRegex, "");
}

std::string JsonAssetSerializer::GenerateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    const char* hexChars = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            uuid += hexChars[dis(gen)];
        }
    }

    return uuid;
}

std::vector<std::string> JsonAssetSerializer::ExtractDependencies(const JsonAsset& asset) const {
    std::vector<std::string> dependencies;

    // Recursively search for path references in the JSON
    std::function<void(const nlohmann::json&)> searchPaths = [&](const nlohmann::json& j) {
        if (j.is_object()) {
            for (auto& [key, value] : j.items()) {
                if (key.find("Path") != std::string::npos || key.find("path") != std::string::npos) {
                    if (value.is_string()) {
                        dependencies.push_back(value.get<std::string>());
                    }
                }
                searchPaths(value);
            }
        } else if (j.is_array()) {
            for (const auto& element : j) {
                searchPaths(element);
            }
        }
    };

    searchPaths(asset.data);
    return dependencies;
}

std::string JsonAssetSerializer::ResolveAssetPath(const std::string& path, const std::string& basePath) const {
    // Simple path resolution - can be enhanced
    if (path.empty() || path[0] == '/' || path[0] == '\\') {
        return path; // Absolute path
    }

    // Relative path
    return basePath + "/" + path;
}

ValidationResult JsonAssetSerializer::ValidateAgainstSchema(const nlohmann::json& json,
                                                           const nlohmann::json& schema) const {
    ValidationResult result;
    // Simplified validation - full implementation would use a JSON schema validator
    result.isValid = true;
    return result;
}

std::vector<std::shared_ptr<IAssetMigration>> JsonAssetSerializer::FindMigrationPath(
    AssetType type, const AssetVersion& from, const AssetVersion& to) const {
    // Simplified - would implement proper migration path finding
    return {};
}

// =============================================================================
// JsonHelpers Implementation
// =============================================================================

namespace JsonHelpers {

nlohmann::json SerializeVec2(const glm::vec2& v) {
    return nlohmann::json::array({v.x, v.y});
}

nlohmann::json SerializeVec3(const glm::vec3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

nlohmann::json SerializeVec4(const glm::vec4& v) {
    return nlohmann::json::array({v.x, v.y, v.z, v.w});
}

nlohmann::json SerializeMat4(const glm::mat4& m) {
    nlohmann::json json = nlohmann::json::array();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            json.push_back(m[i][j]);
        }
    }
    return json;
}

glm::vec2 DeserializeVec2(const nlohmann::json& json) {
    if (json.is_array() && json.size() >= 2) {
        return glm::vec2(json[0], json[1]);
    }
    return glm::vec2(0.0f);
}

glm::vec3 DeserializeVec3(const nlohmann::json& json) {
    if (json.is_array() && json.size() >= 3) {
        return glm::vec3(json[0], json[1], json[2]);
    }
    return glm::vec3(0.0f);
}

glm::vec4 DeserializeVec4(const nlohmann::json& json) {
    if (json.is_array() && json.size() >= 4) {
        return glm::vec4(json[0], json[1], json[2], json[3]);
    }
    return glm::vec4(0.0f);
}

glm::mat4 DeserializeMat4(const nlohmann::json& json) {
    glm::mat4 m(1.0f);
    if (json.is_array() && json.size() >= 16) {
        int idx = 0;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i][j] = json[idx++];
            }
        }
    }
    return m;
}

bool HasField(const nlohmann::json& json, const std::string& field) {
    return json.contains(field);
}

bool HasFieldOfType(const nlohmann::json& json, const std::string& field, nlohmann::json::value_t type) {
    return json.contains(field) && json[field].type() == type;
}

bool IsInRange(float value, float min, float max) {
    return value >= min && value <= max;
}

} // namespace JsonHelpers

// =============================================================================
// LightSerializer Implementation
// =============================================================================

namespace LightSerializer {

nlohmann::json Serialize(const LightData& light) {
    nlohmann::json json;
    json["type"] = "light";
    json["version"] = "1.0";
    json["lightType"] = light.type;
    json["color"] = JsonHelpers::SerializeVec3(light.color);
    json["intensity"] = light.intensity;
    json["temperature"] = light.temperature;
    json["radius"] = light.radius;
    json["position"] = JsonHelpers::SerializeVec3(light.position);
    json["direction"] = JsonHelpers::SerializeVec3(light.direction);
    json["spotAngle"] = light.spotAngle;
    json["castsShadows"] = light.castsShadows;
    if (!light.iesProfile.empty()) json["iesProfile"] = light.iesProfile;
    if (!light.materialFunction.empty()) json["materialFunction"] = light.materialFunction;
    return json;
}

LightData Deserialize(const nlohmann::json& json) {
    LightData light;
    if (json.contains("lightType")) light.type = json["lightType"];
    if (json.contains("color")) light.color = JsonHelpers::DeserializeVec3(json["color"]);
    if (json.contains("intensity")) light.intensity = json["intensity"];
    if (json.contains("temperature")) light.temperature = json["temperature"];
    if (json.contains("radius")) light.radius = json["radius"];
    if (json.contains("position")) light.position = JsonHelpers::DeserializeVec3(json["position"]);
    if (json.contains("direction")) light.direction = JsonHelpers::DeserializeVec3(json["direction"]);
    if (json.contains("spotAngle")) light.spotAngle = json["spotAngle"];
    if (json.contains("castsShadows")) light.castsShadows = json["castsShadows"];
    if (json.contains("iesProfile")) light.iesProfile = json["iesProfile"];
    if (json.contains("materialFunction")) light.materialFunction = json["materialFunction"];
    return light;
}

ValidationResult Validate(const nlohmann::json& json) {
    ValidationResult result;
    if (!JsonHelpers::HasField(json, "lightType")) {
        result.AddError("Missing required field: lightType");
    }
    return result;
}

nlohmann::json GetSchema() {
    return nlohmann::json::object(); // Simplified
}

} // namespace LightSerializer

} // namespace Nova
