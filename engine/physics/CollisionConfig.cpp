#include "CollisionConfig.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Nova {

// ============================================================================
// Error helpers
// ============================================================================

const char* CollisionConfigErrorToString(CollisionConfigError error) noexcept {
    switch (error) {
        case CollisionConfigError::FileNotFound:       return "File not found";
        case CollisionConfigError::ParseError:         return "JSON parse error";
        case CollisionConfigError::InvalidFormat:      return "Invalid format";
        case CollisionConfigError::MissingField:       return "Missing required field";
        case CollisionConfigError::InvalidShapeType:   return "Invalid shape type";
        case CollisionConfigError::MeshLoadFailed:     return "Failed to load mesh";
        case CollisionConfigError::InvalidMeshReference: return "Invalid mesh reference";
        default: return "Unknown error";
    }
}

// ============================================================================
// CollisionConfiguration
// ============================================================================

std::unique_ptr<CollisionBody> CollisionConfiguration::CreateBody() const {
    auto body = std::make_unique<CollisionBody>(bodyType);

    body->SetMass(mass);
    body->SetCollisionLayer(layer);
    body->SetCollisionMask(mask);
    body->SetLinearDamping(linearDamping);
    body->SetAngularDamping(angularDamping);
    body->SetGravityScale(gravityScale);

    for (const auto& shape : shapes) {
        body->AddShape(shape);
    }

    return body;
}

// ============================================================================
// CollisionConfigParser
// ============================================================================

std::expected<CollisionConfiguration, std::string> CollisionConfigParser::Parse(
    const nlohmann::json& json) const
{
    CollisionConfiguration config;

    // Check if we have a "collision" wrapper
    const nlohmann::json* collisionJson = &json;
    if (json.contains("collision")) {
        collisionJson = &json["collision"];
    }

    // Parse body type
    if (collisionJson->contains("body_type")) {
        auto typeOpt = BodyTypeFromString((*collisionJson)["body_type"].get<std::string>());
        if (typeOpt) {
            config.bodyType = *typeOpt;
        }
    }

    // Parse mass
    if (collisionJson->contains("mass")) {
        config.mass = (*collisionJson)["mass"].get<float>();
    }

    // Parse collision layer
    if (collisionJson->contains("layer")) {
        if ((*collisionJson)["layer"].is_string()) {
            config.layer = CollisionLayer::FromString((*collisionJson)["layer"].get<std::string>());
        } else if ((*collisionJson)["layer"].is_number()) {
            config.layer = (*collisionJson)["layer"].get<uint32_t>();
        }
    }

    // Parse collision mask
    if (collisionJson->contains("mask")) {
        config.mask = CollisionLayer::ParseMask((*collisionJson)["mask"]);
    }

    // Parse damping
    if (collisionJson->contains("linear_damping")) {
        config.linearDamping = (*collisionJson)["linear_damping"].get<float>();
    }
    if (collisionJson->contains("angular_damping")) {
        config.angularDamping = (*collisionJson)["angular_damping"].get<float>();
    }

    // Parse gravity scale
    if (collisionJson->contains("gravity_scale")) {
        config.gravityScale = (*collisionJson)["gravity_scale"].get<float>();
    }

    // Parse shapes
    if (collisionJson->contains("shapes") && (*collisionJson)["shapes"].is_array()) {
        for (const auto& shapeJson : (*collisionJson)["shapes"]) {
            auto shapeResult = ParseShape(shapeJson);
            if (!shapeResult) {
                return std::unexpected(shapeResult.error());
            }
            config.shapes.push_back(std::move(*shapeResult));
        }
    }

    // Handle auto-generation
    if (collisionJson->contains("auto_generate")) {
        const auto& autoGen = (*collisionJson)["auto_generate"];
        if (autoGen.contains("from_bounds") && autoGen["from_bounds"].is_array()) {
            const auto& bounds = autoGen["from_bounds"];
            if (bounds.size() >= 2 && bounds[0].is_array() && bounds[1].is_array()) {
                glm::vec3 boundsMin(
                    bounds[0][0].get<float>(),
                    bounds[0][1].get<float>(),
                    bounds[0][2].get<float>()
                );
                glm::vec3 boundsMax(
                    bounds[1][0].get<float>(),
                    bounds[1][1].get<float>(),
                    bounds[1][2].get<float>()
                );

                ShapeType shapeType = ShapeType::Box;
                if (autoGen.contains("shape_type")) {
                    auto typeOpt = ShapeTypeFromString(autoGen["shape_type"].get<std::string>());
                    if (typeOpt) {
                        shapeType = *typeOpt;
                    }
                }

                CollisionShape autoShape = GenerateFromBounds(boundsMin, boundsMax, shapeType);

                if (autoGen.contains("padding")) {
                    float padding = autoGen["padding"].get<float>();
                    // Apply padding to shape
                    if (shapeType == ShapeType::Box) {
                        auto* params = autoShape.GetParams<ShapeParams::Box>();
                        if (params) {
                            params->halfExtents += glm::vec3(padding);
                        }
                    } else if (shapeType == ShapeType::Sphere) {
                        auto* params = autoShape.GetParams<ShapeParams::Sphere>();
                        if (params) {
                            params->radius += padding;
                        }
                    }
                }

                config.shapes.push_back(std::move(autoShape));
            }
        }
    }

    if (config.shapes.empty()) {
        return std::unexpected("No collision shapes defined");
    }

    return config;
}

std::expected<CollisionConfiguration, std::string> CollisionConfigParser::ParseFile(
    const std::filesystem::path& filepath) const
{
    std::filesystem::path fullPath = filepath;
    if (!m_basePath.empty() && filepath.is_relative()) {
        fullPath = m_basePath / filepath;
    }

    if (!std::filesystem::exists(fullPath)) {
        return std::unexpected("File not found: " + fullPath.string());
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return std::unexpected("Could not open file: " + fullPath.string());
    }

    try {
        nlohmann::json json = nlohmann::json::parse(file);
        return Parse(json);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected("JSON parse error: " + std::string(e.what()));
    }
}

std::expected<CollisionConfiguration, std::string> CollisionConfigParser::ParseString(
    const std::string& jsonString) const
{
    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);
        return Parse(json);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected("JSON parse error: " + std::string(e.what()));
    }
}

std::expected<CollisionShape, std::string> CollisionConfigParser::ParseShape(
    const nlohmann::json& json) const
{
    // First try the standard FromJson parser
    auto result = CollisionShape::FromJson(json);
    if (result) {
        // Check for mesh file references
        if (json.contains("mesh_file")) {
            std::string meshPath = json["mesh_file"].get<std::string>();
            auto meshResult = LoadCollisionMesh(meshPath);
            if (!meshResult) {
                return std::unexpected(meshResult.error());
            }
            return *meshResult;
        }
        return result;
    }

    return std::unexpected(result.error());
}

std::expected<CollisionShape, std::string> CollisionConfigParser::ParseCompoundShape(
    const nlohmann::json& json) const
{
    if (!json.contains("children") || !json["children"].is_array()) {
        return std::unexpected("Compound shape requires 'children' array");
    }

    ShapeParams::Compound compoundParams;

    for (const auto& childJson : json["children"]) {
        auto childResult = ParseShape(childJson);
        if (!childResult) {
            return std::unexpected("Failed to parse child shape: " + childResult.error());
        }
        compoundParams.children.push_back(
            std::make_shared<CollisionShape>(std::move(*childResult)));
    }

    CollisionShape shape(ShapeType::Compound);
    shape.SetParams(compoundParams);
    return shape;
}

CollisionShape CollisionConfigParser::GenerateFromBounds(
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    ShapeType shapeType)
{
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 extents = (boundsMax - boundsMin) * 0.5f;

    CollisionShape shape;

    switch (shapeType) {
        case ShapeType::Box: {
            shape = CollisionShape::CreateBox(extents);
            break;
        }
        case ShapeType::Sphere: {
            float radius = glm::length(extents);
            shape = CollisionShape::CreateSphere(radius);
            break;
        }
        case ShapeType::Capsule: {
            // Use largest horizontal extent as radius, height as capsule height
            float radius = std::max(extents.x, extents.z);
            float height = std::max(extents.y * 2.0f - radius * 2.0f, 0.0f);
            shape = CollisionShape::CreateCapsule(radius, height);
            break;
        }
        case ShapeType::Cylinder: {
            float radius = std::max(extents.x, extents.z);
            float height = extents.y * 2.0f;
            shape = CollisionShape::CreateCylinder(radius, height);
            break;
        }
        default:
            shape = CollisionShape::CreateBox(extents);
            break;
    }

    // Set offset if center is not at origin
    if (center != glm::vec3(0.0f)) {
        ShapeTransform transform;
        transform.position = center;
        shape.SetLocalTransform(transform);
    }

    return shape;
}

CollisionShape CollisionConfigParser::GenerateFromMesh(
    const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices,
    bool convex)
{
    if (vertices.empty()) {
        return CollisionShape::CreateBox(glm::vec3(0.5f));
    }

    if (convex) {
        // Generate convex hull
        ShapeParams::ConvexHull params;
        params.vertices = vertices;

        // Simple convex hull - just use all vertices for now
        // A real implementation would compute the actual convex hull

        CollisionShape shape(ShapeType::ConvexHull);
        shape.SetParams(params);
        return shape;
    } else {
        // Generate triangle mesh
        ShapeParams::TriangleMesh params;
        params.vertices = vertices;
        params.indices = indices;

        CollisionShape shape(ShapeType::TriangleMesh);
        shape.SetParams(params);
        return shape;
    }
}

std::expected<CollisionShape, std::string> CollisionConfigParser::LoadCollisionMesh(
    const std::filesystem::path& filepath) const
{
    std::filesystem::path fullPath = ResolvePath(filepath.string());

    if (!std::filesystem::exists(fullPath)) {
        return std::unexpected("Mesh file not found: " + fullPath.string());
    }

    // Simple OBJ loader for collision meshes
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return std::unexpected("Could not open mesh file: " + fullPath.string());
    }

    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
        } else if (prefix == "f") {
            // Parse face indices (handle v, v/vt, v/vt/vn, v//vn formats)
            std::string token;
            std::vector<uint32_t> faceIndices;

            while (iss >> token) {
                size_t slashPos = token.find('/');
                std::string indexStr = (slashPos != std::string::npos) ?
                    token.substr(0, slashPos) : token;

                int idx = std::stoi(indexStr);
                // OBJ indices are 1-based
                faceIndices.push_back(static_cast<uint32_t>(idx > 0 ? idx - 1 : vertices.size() + idx));
            }

            // Triangulate face (fan triangulation)
            for (size_t i = 2; i < faceIndices.size(); ++i) {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i - 1]);
                indices.push_back(faceIndices[i]);
            }
        }
    }

    if (vertices.empty()) {
        return std::unexpected("No vertices found in mesh file");
    }

    // Determine if this should be a convex hull or triangle mesh
    // Use convex hull for small meshes, triangle mesh for large ones
    bool useConvex = vertices.size() <= 64;

    CollisionShape shape = GenerateFromMesh(vertices, indices, useConvex);

    // Store the file path for serialization
    if (auto* params = shape.GetParams<ShapeParams::TriangleMesh>()) {
        params->meshFilePath = filepath.string();
    }

    return shape;
}

std::filesystem::path CollisionConfigParser::ResolvePath(const std::string& path) const {
    std::filesystem::path fsPath(path);
    if (!m_basePath.empty() && fsPath.is_relative()) {
        return m_basePath / fsPath;
    }
    return fsPath;
}

// ============================================================================
// CollisionConfigCache
// ============================================================================

CollisionConfigCache& CollisionConfigCache::Instance() {
    static CollisionConfigCache instance;
    return instance;
}

const CollisionConfiguration* CollisionConfigCache::Get(const std::filesystem::path& filepath) {
    std::string key = filepath.string();

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return &it->second;
    }

    auto result = m_parser.ParseFile(filepath);
    if (!result) {
        return nullptr;
    }

    auto [inserted, _] = m_cache.emplace(key, std::move(*result));
    return &inserted->second;
}

const CollisionConfiguration* CollisionConfigCache::GetFromJson(
    const std::string& key,
    const nlohmann::json& json)
{
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return &it->second;
    }

    auto result = m_parser.Parse(json);
    if (!result) {
        return nullptr;
    }

    auto [inserted, _] = m_cache.emplace(key, std::move(*result));
    return &inserted->second;
}

void CollisionConfigCache::Clear() {
    m_cache.clear();
}

void CollisionConfigCache::Remove(const std::string& key) {
    m_cache.erase(key);
}

// ============================================================================
// CollisionSchema
// ============================================================================

namespace CollisionSchema {

bool Validate(const nlohmann::json& json, std::string& error) {
    const nlohmann::json* collision = &json;

    if (json.contains("collision")) {
        collision = &json["collision"];
    }

    // Must have shapes or auto_generate
    bool hasShapes = collision->contains("shapes") && (*collision)["shapes"].is_array();
    bool hasAutoGen = collision->contains("auto_generate");

    if (!hasShapes && !hasAutoGen) {
        error = "Collision config must have 'shapes' array or 'auto_generate' object";
        return false;
    }

    // Validate shapes
    if (hasShapes) {
        for (size_t i = 0; i < (*collision)["shapes"].size(); ++i) {
            const auto& shape = (*collision)["shapes"][i];

            if (!shape.contains("type")) {
                error = "Shape " + std::to_string(i) + " missing 'type' field";
                return false;
            }

            std::string type = shape["type"].get<std::string>();
            if (!ShapeTypeFromString(type)) {
                error = "Shape " + std::to_string(i) + " has invalid type: " + type;
                return false;
            }
        }
    }

    // Validate body_type if present
    if (collision->contains("body_type")) {
        std::string bodyType = (*collision)["body_type"].get<std::string>();
        if (!BodyTypeFromString(bodyType)) {
            error = "Invalid body_type: " + bodyType;
            return false;
        }
    }

    return true;
}

nlohmann::json GetDefault() {
    return nlohmann::json{
        {"collision", {
            {"shapes", nlohmann::json::array({
                {
                    {"type", "box"},
                    {"half_extents", {0.5f, 0.5f, 0.5f}}
                }
            })},
            {"body_type", "static"},
            {"layer", "default"},
            {"mask", {"all"}}
        }}
    };
}

} // namespace CollisionSchema

} // namespace Nova
