#include "SDFSerializer.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace Nova {

// Simple JSON writing helpers (using manual string building for portability)
namespace {

std::string Indent(int level) {
    return std::string(level * 2, ' ');
}

std::string EscapeString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string FloatStr(float f) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", f);
    return buf;
}

// Very basic JSON parsing helpers
std::string GetJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";

    size_t start = pos + 1;
    size_t end = start;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\') end++;
        end++;
    }

    return json.substr(start, end - start);
}

float GetJsonFloat(const std::string& json, const std::string& key, float defaultVal = 0.0f) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;

    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    return std::stof(json.substr(pos));
}

bool GetJsonBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;

    return json.find("true", pos) < json.find("false", pos);
}

int GetJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;

    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    return std::stoi(json.substr(pos));
}

std::string GetJsonObject(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";

    int depth = 1;
    size_t start = pos;
    pos++;

    while (pos < json.size() && depth > 0) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') depth--;
        else if (json[pos] == '"') {
            pos++;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\') pos++;
                pos++;
            }
        }
        pos++;
    }

    return json.substr(start, pos - start);
}

std::string GetJsonArray(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find('[', pos);
    if (pos == std::string::npos) return "";

    int depth = 1;
    size_t start = pos;
    pos++;

    while (pos < json.size() && depth > 0) {
        if (json[pos] == '[') depth++;
        else if (json[pos] == ']') depth--;
        else if (json[pos] == '"') {
            pos++;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\') pos++;
                pos++;
            }
        }
        pos++;
    }

    return json.substr(start, pos - start);
}

std::vector<std::string> GetJsonArrayElements(const std::string& arrayJson) {
    std::vector<std::string> result;
    if (arrayJson.size() < 2) return result;

    size_t pos = 1;  // Skip opening [

    while (pos < arrayJson.size() - 1) {
        // Skip whitespace
        while (pos < arrayJson.size() && (arrayJson[pos] == ' ' || arrayJson[pos] == '\t' || arrayJson[pos] == '\n' || arrayJson[pos] == ',')) pos++;

        if (pos >= arrayJson.size() - 1 || arrayJson[pos] == ']') break;

        size_t start = pos;
        int depth = 0;

        // Find element end
        while (pos < arrayJson.size()) {
            char c = arrayJson[pos];
            if (c == '{' || c == '[') depth++;
            else if (c == '}' || c == ']') {
                if (depth == 0) break;
                depth--;
            } else if (c == ',' && depth == 0) {
                break;
            } else if (c == '"') {
                pos++;
                while (pos < arrayJson.size() && arrayJson[pos] != '"') {
                    if (arrayJson[pos] == '\\') pos++;
                    pos++;
                }
            }
            pos++;
        }

        std::string element = arrayJson.substr(start, pos - start);
        // Trim
        size_t first = element.find_first_not_of(" \t\n");
        if (first != std::string::npos) {
            size_t last = element.find_last_not_of(" \t\n,");
            element = element.substr(first, last - first + 1);
        }

        if (!element.empty()) {
            result.push_back(element);
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// Type conversion helpers
// ============================================================================

const char* SDFSerializer::PrimitiveTypeToString(SDFPrimitiveType type) {
    switch (type) {
        case SDFPrimitiveType::Sphere: return "sphere";
        case SDFPrimitiveType::Box: return "box";
        case SDFPrimitiveType::Cylinder: return "cylinder";
        case SDFPrimitiveType::Capsule: return "capsule";
        case SDFPrimitiveType::Cone: return "cone";
        case SDFPrimitiveType::Torus: return "torus";
        case SDFPrimitiveType::Plane: return "plane";
        case SDFPrimitiveType::RoundedBox: return "rounded_box";
        case SDFPrimitiveType::Ellipsoid: return "ellipsoid";
        case SDFPrimitiveType::Pyramid: return "pyramid";
        case SDFPrimitiveType::Prism: return "prism";
        case SDFPrimitiveType::Custom: return "custom";
    }
    return "sphere";
}

SDFPrimitiveType SDFSerializer::PrimitiveTypeFromString(const std::string& str) {
    if (str == "sphere") return SDFPrimitiveType::Sphere;
    if (str == "box") return SDFPrimitiveType::Box;
    if (str == "cylinder") return SDFPrimitiveType::Cylinder;
    if (str == "capsule") return SDFPrimitiveType::Capsule;
    if (str == "cone") return SDFPrimitiveType::Cone;
    if (str == "torus") return SDFPrimitiveType::Torus;
    if (str == "plane") return SDFPrimitiveType::Plane;
    if (str == "rounded_box") return SDFPrimitiveType::RoundedBox;
    if (str == "ellipsoid") return SDFPrimitiveType::Ellipsoid;
    if (str == "pyramid") return SDFPrimitiveType::Pyramid;
    if (str == "prism") return SDFPrimitiveType::Prism;
    if (str == "custom") return SDFPrimitiveType::Custom;
    return SDFPrimitiveType::Sphere;
}

const char* SDFSerializer::CSGOperationToString(CSGOperation op) {
    switch (op) {
        case CSGOperation::Union: return "union";
        case CSGOperation::Subtraction: return "subtraction";
        case CSGOperation::Intersection: return "intersection";
        case CSGOperation::SmoothUnion: return "smooth_union";
        case CSGOperation::SmoothSubtraction: return "smooth_subtraction";
        case CSGOperation::SmoothIntersection: return "smooth_intersection";
    }
    return "union";
}

CSGOperation SDFSerializer::CSGOperationFromString(const std::string& str) {
    if (str == "union") return CSGOperation::Union;
    if (str == "subtraction") return CSGOperation::Subtraction;
    if (str == "intersection") return CSGOperation::Intersection;
    if (str == "smooth_union") return CSGOperation::SmoothUnion;
    if (str == "smooth_subtraction") return CSGOperation::SmoothSubtraction;
    if (str == "smooth_intersection") return CSGOperation::SmoothIntersection;
    return CSGOperation::Union;
}

std::string SDFSerializer::Vec3ToJson(const glm::vec3& v) {
    return "[" + FloatStr(v.x) + ", " + FloatStr(v.y) + ", " + FloatStr(v.z) + "]";
}

glm::vec3 SDFSerializer::Vec3FromJson(const std::string& json) {
    glm::vec3 v(0.0f);
    if (json.empty() || json[0] != '[') return v;

    std::string inner = json.substr(1, json.size() - 2);
    std::istringstream ss(inner);
    char comma;
    ss >> v.x >> comma >> v.y >> comma >> v.z;
    return v;
}

std::string SDFSerializer::Vec4ToJson(const glm::vec4& v) {
    return "[" + FloatStr(v.x) + ", " + FloatStr(v.y) + ", " + FloatStr(v.z) + ", " + FloatStr(v.w) + "]";
}

glm::vec4 SDFSerializer::Vec4FromJson(const std::string& json) {
    glm::vec4 v(0.0f);
    if (json.empty() || json[0] != '[') return v;

    std::string inner = json.substr(1, json.size() - 2);
    std::istringstream ss(inner);
    char comma;
    ss >> v.x >> comma >> v.y >> comma >> v.z >> comma >> v.w;
    return v;
}

std::string SDFSerializer::QuatToJson(const glm::quat& q) {
    return "[" + FloatStr(q.w) + ", " + FloatStr(q.x) + ", " + FloatStr(q.y) + ", " + FloatStr(q.z) + "]";
}

glm::quat SDFSerializer::QuatFromJson(const std::string& json) {
    glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
    if (json.empty() || json[0] != '[') return q;

    std::string inner = json.substr(1, json.size() - 2);
    std::istringstream ss(inner);
    char comma;
    ss >> q.w >> comma >> q.x >> comma >> q.y >> comma >> q.z;
    return q;
}

std::string SDFSerializer::TransformToJson(const SDFTransform& transform) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "      \"position\": " << Vec3ToJson(transform.position) << ",\n";
    ss << "      \"rotation\": " << QuatToJson(transform.rotation) << ",\n";
    ss << "      \"scale\": " << Vec3ToJson(transform.scale) << "\n";
    ss << "    }";
    return ss.str();
}

SDFTransform SDFSerializer::TransformFromJson(const std::string& json) {
    SDFTransform transform;
    transform.position = Vec3FromJson(GetJsonArray(json, "position"));
    transform.rotation = QuatFromJson(GetJsonArray(json, "rotation"));
    transform.scale = Vec3FromJson(GetJsonArray(json, "scale"));
    if (transform.scale == glm::vec3(0.0f)) transform.scale = glm::vec3(1.0f);
    return transform;
}

std::string SDFSerializer::MaterialToJson(const SDFMaterial& material) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "      \"base_color\": " << Vec4ToJson(material.baseColor) << ",\n";
    ss << "      \"metallic\": " << FloatStr(material.metallic) << ",\n";
    ss << "      \"roughness\": " << FloatStr(material.roughness) << ",\n";
    ss << "      \"emissive\": " << FloatStr(material.emissive) << ",\n";
    ss << "      \"emissive_color\": " << Vec3ToJson(material.emissiveColor);
    if (!material.texturePath.empty()) {
        ss << ",\n      \"texture\": \"" << EscapeString(material.texturePath) << "\"";
    }
    if (!material.normalMapPath.empty()) {
        ss << ",\n      \"normal_map\": \"" << EscapeString(material.normalMapPath) << "\"";
    }
    ss << "\n    }";
    return ss.str();
}

SDFMaterial SDFSerializer::MaterialFromJson(const std::string& json) {
    SDFMaterial material;
    material.baseColor = Vec4FromJson(GetJsonArray(json, "base_color"));
    if (material.baseColor == glm::vec4(0.0f)) material.baseColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    material.metallic = GetJsonFloat(json, "metallic", 0.0f);
    material.roughness = GetJsonFloat(json, "roughness", 0.5f);
    material.emissive = GetJsonFloat(json, "emissive", 0.0f);
    material.emissiveColor = Vec3FromJson(GetJsonArray(json, "emissive_color"));
    material.texturePath = GetJsonString(json, "texture");
    material.normalMapPath = GetJsonString(json, "normal_map");
    return material;
}

std::string SDFSerializer::ParametersToJson(const SDFParameters& params) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "      \"radius\": " << FloatStr(params.radius) << ",\n";
    ss << "      \"dimensions\": " << Vec3ToJson(params.dimensions) << ",\n";
    ss << "      \"corner_radius\": " << FloatStr(params.cornerRadius) << ",\n";
    ss << "      \"height\": " << FloatStr(params.height) << ",\n";
    ss << "      \"top_radius\": " << FloatStr(params.topRadius) << ",\n";
    ss << "      \"bottom_radius\": " << FloatStr(params.bottomRadius) << ",\n";
    ss << "      \"major_radius\": " << FloatStr(params.majorRadius) << ",\n";
    ss << "      \"minor_radius\": " << FloatStr(params.minorRadius) << ",\n";
    ss << "      \"radii\": " << Vec3ToJson(params.radii) << ",\n";
    ss << "      \"sides\": " << params.sides << ",\n";
    ss << "      \"smoothness\": " << FloatStr(params.smoothness) << "\n";
    ss << "    }";
    return ss.str();
}

SDFParameters SDFSerializer::ParametersFromJson(const std::string& json) {
    SDFParameters params;
    params.radius = GetJsonFloat(json, "radius", 0.5f);
    params.dimensions = Vec3FromJson(GetJsonArray(json, "dimensions"));
    if (params.dimensions == glm::vec3(0.0f)) params.dimensions = glm::vec3(1.0f);
    params.cornerRadius = GetJsonFloat(json, "corner_radius", 0.0f);
    params.height = GetJsonFloat(json, "height", 1.0f);
    params.topRadius = GetJsonFloat(json, "top_radius", 0.5f);
    params.bottomRadius = GetJsonFloat(json, "bottom_radius", 0.5f);
    params.majorRadius = GetJsonFloat(json, "major_radius", 0.5f);
    params.minorRadius = GetJsonFloat(json, "minor_radius", 0.1f);
    params.radii = Vec3FromJson(GetJsonArray(json, "radii"));
    if (params.radii == glm::vec3(0.0f)) params.radii = glm::vec3(0.5f, 0.3f, 0.4f);
    params.sides = GetJsonInt(json, "sides", 6);
    params.smoothness = GetJsonFloat(json, "smoothness", 0.1f);
    return params;
}

// ============================================================================
// Primitive Serialization
// ============================================================================

std::string SerializePrimitiveRecursive(const SDFPrimitive& prim, int indent) {
    std::ostringstream ss;
    std::string ind = Indent(indent);

    ss << ind << "{\n";
    ss << ind << "  \"name\": \"" << EscapeString(prim.GetName()) << "\",\n";
    ss << ind << "  \"type\": \"" << SDFSerializer::PrimitiveTypeToString(prim.GetType()) << "\",\n";
    ss << ind << "  \"csg_operation\": \"" << SDFSerializer::CSGOperationToString(prim.GetCSGOperation()) << "\",\n";
    ss << ind << "  \"visible\": " << (prim.IsVisible() ? "true" : "false") << ",\n";
    ss << ind << "  \"locked\": " << (prim.IsLocked() ? "true" : "false") << ",\n";
    ss << ind << "  \"transform\": " << SDFSerializer::TransformToJson(prim.GetLocalTransform()) << ",\n";
    ss << ind << "  \"parameters\": " << SDFSerializer::ParametersToJson(prim.GetParameters()) << ",\n";
    ss << ind << "  \"material\": " << SDFSerializer::MaterialToJson(prim.GetMaterial());

    const auto& children = prim.GetChildren();
    if (!children.empty()) {
        ss << ",\n" << ind << "  \"children\": [\n";
        for (size_t i = 0; i < children.size(); ++i) {
            ss << SerializePrimitiveRecursive(*children[i], indent + 2);
            if (i < children.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << ind << "  ]";
    }

    ss << "\n" << ind << "}";
    return ss.str();
}

std::string SDFSerializer::PrimitiveToJson(const SDFPrimitive& primitive) {
    return SerializePrimitiveRecursive(primitive, 0);
}

std::unique_ptr<SDFPrimitive> DeserializePrimitiveRecursive(const std::string& json) {
    std::string name = GetJsonString(json, "name");
    SDFPrimitiveType type = SDFSerializer::PrimitiveTypeFromString(GetJsonString(json, "type"));

    auto prim = std::make_unique<SDFPrimitive>(name, type);
    prim->SetCSGOperation(SDFSerializer::CSGOperationFromString(GetJsonString(json, "csg_operation")));
    prim->SetVisible(GetJsonBool(json, "visible", true));
    prim->SetLocked(GetJsonBool(json, "locked", false));
    prim->SetLocalTransform(SDFSerializer::TransformFromJson(GetJsonObject(json, "transform")));
    prim->SetParameters(SDFSerializer::ParametersFromJson(GetJsonObject(json, "parameters")));
    prim->SetMaterial(SDFSerializer::MaterialFromJson(GetJsonObject(json, "material")));

    std::string childrenJson = GetJsonArray(json, "children");
    if (!childrenJson.empty()) {
        auto elements = GetJsonArrayElements(childrenJson);
        for (const auto& childJson : elements) {
            auto child = DeserializePrimitiveRecursive(childJson);
            if (child) {
                prim->AddChild(std::move(child));
            }
        }
    }

    return prim;
}

std::unique_ptr<SDFPrimitive> SDFSerializer::PrimitiveFromJson(const std::string& json) {
    return DeserializePrimitiveRecursive(json);
}

// ============================================================================
// Model Serialization
// ============================================================================

std::string SDFSerializer::ModelToJson(const SDFModel& model) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << EscapeString(model.GetName()) << "\",\n";
    ss << "  \"id\": " << model.GetId() << ",\n";

    // Mesh settings
    const auto& settings = model.GetMeshSettings();
    ss << "  \"mesh_settings\": {\n";
    ss << "    \"resolution\": " << settings.resolution << ",\n";
    ss << "    \"bounds_padding\": " << FloatStr(settings.boundsPadding) << ",\n";
    ss << "    \"iso_level\": " << FloatStr(settings.isoLevel) << ",\n";
    ss << "    \"smooth_normals\": " << (settings.smoothNormals ? "true" : "false") << ",\n";
    ss << "    \"generate_uvs\": " << (settings.generateUVs ? "true" : "false") << "\n";
    ss << "  },\n";

    // Root primitive
    if (model.GetRoot()) {
        ss << "  \"root\": " << PrimitiveToJson(*model.GetRoot()) << ",\n";
    }

    // Paint layers
    const auto& layers = model.GetPaintLayers();
    if (!layers.empty()) {
        ss << "  \"paint_layers\": [\n";
        for (size_t i = 0; i < layers.size(); ++i) {
            ss << "    {\n";
            ss << "      \"name\": \"" << EscapeString(layers[i].name) << "\",\n";
            ss << "      \"width\": " << layers[i].width << ",\n";
            ss << "      \"height\": " << layers[i].height << ",\n";
            ss << "      \"opacity\": " << FloatStr(layers[i].opacity) << ",\n";
            ss << "      \"visible\": " << (layers[i].visible ? "true" : "false") << "\n";
            // Note: actual paint data would be stored separately or base64 encoded
            ss << "    }";
            if (i < layers.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ],\n";
    }

    // Base texture
    if (!model.GetBaseTexturePath().empty()) {
        ss << "  \"base_texture\": \"" << EscapeString(model.GetBaseTexturePath()) << "\"\n";
    } else {
        // Remove trailing comma from previous line
        ss.seekp(-2, std::ios_base::cur);
        ss << "\n";
    }

    ss << "}";
    return ss.str();
}

std::unique_ptr<SDFModel> SDFSerializer::ModelFromJson(const std::string& json) {
    auto model = std::make_unique<SDFModel>(GetJsonString(json, "name"));

    // Mesh settings
    std::string settingsJson = GetJsonObject(json, "mesh_settings");
    if (!settingsJson.empty()) {
        SDFMeshSettings settings;
        settings.resolution = GetJsonInt(settingsJson, "resolution", 64);
        settings.boundsPadding = GetJsonFloat(settingsJson, "bounds_padding", 0.1f);
        settings.isoLevel = GetJsonFloat(settingsJson, "iso_level", 0.0f);
        settings.smoothNormals = GetJsonBool(settingsJson, "smooth_normals", true);
        settings.generateUVs = GetJsonBool(settingsJson, "generate_uvs", true);
        model->SetMeshSettings(settings);
    }

    // Root primitive
    std::string rootJson = GetJsonObject(json, "root");
    if (!rootJson.empty()) {
        model->SetRoot(PrimitiveFromJson(rootJson));
    }

    // Base texture
    std::string baseTex = GetJsonString(json, "base_texture");
    if (!baseTex.empty()) {
        model->SetBaseTexturePath(baseTex);
    }

    return model;
}

bool SDFSerializer::SaveModel(const SDFModel& model, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << ModelToJson(model);
    return true;
}

std::unique_ptr<SDFModel> SDFSerializer::LoadModel(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::ostringstream ss;
    ss << file.rdbuf();
    return ModelFromJson(ss.str());
}

// ============================================================================
// Pose Serialization
// ============================================================================

std::string SDFSerializer::PoseToJson(const SDFPose& pose) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << EscapeString(pose.name) << "\",\n";
    ss << "  \"category\": \"" << EscapeString(pose.category) << "\",\n";
    ss << "  \"description\": \"" << EscapeString(pose.description) << "\",\n";
    ss << "  \"timestamp\": " << pose.timestamp << ",\n";

    // Tags
    ss << "  \"tags\": [";
    for (size_t i = 0; i < pose.tags.size(); ++i) {
        ss << "\"" << EscapeString(pose.tags[i]) << "\"";
        if (i < pose.tags.size() - 1) ss << ", ";
    }
    ss << "],\n";

    // Transforms
    ss << "  \"transforms\": {\n";
    size_t i = 0;
    for (const auto& [name, transform] : pose.transforms) {
        ss << "    \"" << EscapeString(name) << "\": " << TransformToJson(transform);
        if (++i < pose.transforms.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";

    ss << "}";
    return ss.str();
}

SDFPose SDFSerializer::PoseFromJson(const std::string& json) {
    SDFPose pose;
    pose.name = GetJsonString(json, "name");
    pose.category = GetJsonString(json, "category");
    pose.description = GetJsonString(json, "description");
    pose.timestamp = static_cast<uint64_t>(GetJsonFloat(json, "timestamp", 0));

    // Parse transforms object
    std::string transformsJson = GetJsonObject(json, "transforms");
    // This is simplified - a full implementation would parse each key-value pair

    return pose;
}

// ============================================================================
// Animation Clip Serialization
// ============================================================================

std::string SDFSerializer::AnimationClipToJson(const SDFAnimationClip& clip) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << EscapeString(clip.GetName()) << "\",\n";
    ss << "  \"duration\": " << FloatStr(clip.GetDuration()) << ",\n";
    ss << "  \"frame_rate\": " << FloatStr(clip.GetFrameRate()) << ",\n";
    ss << "  \"looping\": " << (clip.IsLooping() ? "true" : "false") << ",\n";

    // Keyframes
    ss << "  \"keyframes\": [\n";
    const auto& keyframes = clip.GetKeyframes();
    for (size_t i = 0; i < keyframes.size(); ++i) {
        const auto& kf = keyframes[i];
        ss << "    {\n";
        ss << "      \"time\": " << FloatStr(kf.time) << ",\n";
        ss << "      \"easing\": \"" << EscapeString(kf.easing) << "\",\n";
        ss << "      \"transforms\": {\n";

        size_t j = 0;
        for (const auto& [name, transform] : kf.transforms) {
            ss << "        \"" << EscapeString(name) << "\": " << TransformToJson(transform);
            if (++j < kf.transforms.size()) ss << ",";
            ss << "\n";
        }
        ss << "      }\n";
        ss << "    }";
        if (i < keyframes.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}";
    return ss.str();
}

std::unique_ptr<SDFAnimationClip> SDFSerializer::AnimationClipFromJson(const std::string& json) {
    auto clip = std::make_unique<SDFAnimationClip>(GetJsonString(json, "name"));
    clip->SetDuration(GetJsonFloat(json, "duration", 1.0f));
    clip->SetFrameRate(GetJsonFloat(json, "frame_rate", 30.0f));
    clip->SetLooping(GetJsonBool(json, "looping", true));

    // Parse keyframes
    std::string keyframesJson = GetJsonArray(json, "keyframes");
    auto elements = GetJsonArrayElements(keyframesJson);

    for (const auto& kfJson : elements) {
        float time = GetJsonFloat(kfJson, "time", 0.0f);
        auto* kf = clip->AddKeyframe(time);
        if (kf) {
            kf->easing = GetJsonString(kfJson, "easing");
            if (kf->easing.empty()) kf->easing = "linear";
            // Transform parsing would go here
        }
    }

    return clip;
}

bool SDFSerializer::SaveAnimationClip(const SDFAnimationClip& clip, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << AnimationClipToJson(clip);
    return true;
}

std::unique_ptr<SDFAnimationClip> SDFSerializer::LoadAnimationClip(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::ostringstream ss;
    ss << file.rdbuf();
    return AnimationClipFromJson(ss.str());
}

// ============================================================================
// Entity Integration
// ============================================================================

std::string SDFSerializer::CreateEntitySDFSection(
    const SDFModel& model,
    const SDFPoseLibrary* poseLibrary,
    const std::vector<const SDFAnimationClip*>& animations,
    const SDFAnimationStateMachine* stateMachine) {

    std::ostringstream ss;

    // SDF Model
    ss << "\"sdf_model\": " << ModelToJson(model);

    // Poses
    if (poseLibrary && !poseLibrary->GetAllPoses().empty()) {
        ss << ",\n\"sdf_poses\": [\n";
        const auto& poses = poseLibrary->GetAllPoses();
        for (size_t i = 0; i < poses.size(); ++i) {
            ss << "  " << PoseToJson(poses[i]);
            if (i < poses.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "]";
    }

    // Animations
    if (!animations.empty()) {
        ss << ",\n\"sdf_animations\": [\n";
        for (size_t i = 0; i < animations.size(); ++i) {
            if (animations[i]) {
                ss << "  " << AnimationClipToJson(*animations[i]);
                if (i < animations.size() - 1) ss << ",";
                ss << "\n";
            }
        }
        ss << "]";
    }

    // State machine
    if (stateMachine) {
        ss << ",\n\"sdf_state_machine\": " << StateMachineToJson(*stateMachine);
    }

    return ss.str();
}

SDFSerializer::EntitySDFData SDFSerializer::ParseEntitySDFSection(const std::string& json) {
    EntitySDFData data;

    // Parse model
    std::string modelJson = GetJsonObject(json, "sdf_model");
    if (!modelJson.empty()) {
        data.model = ModelFromJson(modelJson);
    }

    // Parse poses
    std::string posesJson = GetJsonArray(json, "sdf_poses");
    if (!posesJson.empty()) {
        data.poseLibrary = std::make_unique<SDFPoseLibrary>();
        auto poseElements = GetJsonArrayElements(posesJson);
        for (const auto& poseJson : poseElements) {
            SDFPose pose = PoseFromJson(poseJson);
            data.poseLibrary->SavePose(pose.name, pose.transforms, pose.category);
        }
    }

    // Parse animations
    std::string animsJson = GetJsonArray(json, "sdf_animations");
    if (!animsJson.empty()) {
        auto animElements = GetJsonArrayElements(animsJson);
        for (const auto& animJson : animElements) {
            auto clip = AnimationClipFromJson(animJson);
            if (clip) {
                data.animations.push_back(std::move(clip));
            }
        }
    }

    return data;
}

bool SDFSerializer::UpdateEntityJson(
    const std::string& jsonPath,
    const SDFModel& model,
    const SDFPoseLibrary* poseLibrary,
    const std::vector<const SDFAnimationClip*>& animations,
    const SDFAnimationStateMachine* stateMachine) {

    // Read existing file
    std::ifstream inFile(jsonPath);
    if (!inFile.is_open()) return false;

    std::ostringstream buffer;
    buffer << inFile.rdbuf();
    std::string existingJson = buffer.str();
    inFile.close();

    // Find insertion point (before closing brace)
    size_t insertPos = existingJson.rfind('}');
    if (insertPos == std::string::npos) return false;

    // Check if we need a comma
    size_t lastContent = existingJson.find_last_not_of(" \t\n\r", insertPos - 1);
    bool needsComma = (lastContent != std::string::npos && existingJson[lastContent] != '{' && existingJson[lastContent] != ',');

    // Build new content
    std::ostringstream newContent;
    if (needsComma) newContent << ",";
    newContent << "\n\n  // SDF Model Definition (Generated)\n  ";
    newContent << CreateEntitySDFSection(model, poseLibrary, animations, stateMachine);
    newContent << "\n";

    // Insert new content
    std::string result = existingJson.substr(0, insertPos) + newContent.str() + existingJson.substr(insertPos);

    // Write back
    std::ofstream outFile(jsonPath);
    if (!outFile.is_open()) return false;
    outFile << result;

    return true;
}

SDFSerializer::EntitySDFData SDFSerializer::LoadEntitySDF(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) return {};

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return ParseEntitySDFSection(buffer.str());
}

std::string SDFSerializer::StateMachineToJson(const SDFAnimationStateMachine& sm) {
    std::ostringstream ss;
    ss << "{\n";

    // States
    ss << "  \"states\": [\n";
    const auto& states = sm.GetStates();
    size_t i = 0;
    for (const auto& [name, state] : states) {
        ss << "    {\n";
        ss << "      \"name\": \"" << EscapeString(state.name) << "\",\n";
        ss << "      \"clip\": \"" << EscapeString(state.clip ? state.clip->GetName() : "") << "\",\n";
        ss << "      \"speed\": " << FloatStr(state.speed) << ",\n";
        ss << "      \"loop\": " << (state.loop ? "true" : "false") << "\n";
        ss << "    }";
        if (++i < states.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}";
    return ss.str();
}

std::unique_ptr<SDFAnimationStateMachine> SDFSerializer::StateMachineFromJson(
    const std::string& json,
    const std::unordered_map<std::string, std::shared_ptr<SDFAnimationClip>>& clips) {

    auto sm = std::make_unique<SDFAnimationStateMachine>();

    std::string statesJson = GetJsonArray(json, "states");
    auto stateElements = GetJsonArrayElements(statesJson);

    for (const auto& stateJson : stateElements) {
        std::string name = GetJsonString(stateJson, "name");
        std::string clipName = GetJsonString(stateJson, "clip");

        std::shared_ptr<SDFAnimationClip> clip;
        auto it = clips.find(clipName);
        if (it != clips.end()) {
            clip = it->second;
        }

        auto* state = sm->AddState(name, clip);
        if (state) {
            state->speed = GetJsonFloat(stateJson, "speed", 1.0f);
            state->loop = GetJsonBool(stateJson, "loop", true);
        }
    }

    return sm;
}

} // namespace Nova
