#include "ModelImporter.hpp"
#include "../animation/Skeleton.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <queue>
#include <set>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Constructor/Destructor
// ============================================================================

ModelImporter::ModelImporter() = default;
ModelImporter::~ModelImporter() = default;

// ============================================================================
// Single Model Import
// ============================================================================

ImportedModel ModelImporter::Import(const std::string& path,
                                     const ModelImportSettings& settings,
                                     ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;
    result.success = false;

    if (!fs::exists(path)) {
        result.errorMessage = "File not found: " + path;
        if (progress) progress->Error(result.errorMessage);
        return result;
    }

    // Setup progress stages
    if (progress) {
        progress->AddStage("load", "Loading model file", 2.0f);
        progress->AddStage("process", "Processing meshes", 3.0f);
        progress->AddStage("optimize", "Optimizing", 2.0f);
        progress->AddStage("collision", "Generating collision", 1.0f);
        progress->AddStage("output", "Finalizing", 1.0f);
        progress->SetStatus(ImportStatus::InProgress);
        progress->StartTiming();
    }

    // Detect format and load
    if (progress) progress->BeginStage("load");

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".obj") {
        result = LoadOBJ(path, progress);
    } else if (ext == ".gltf" || ext == ".glb") {
        result = LoadGLTF(path, progress);
    } else if (ext == ".fbx") {
        result = LoadFBX(path, progress);
    } else if (ext == ".dae") {
        result = LoadDAE(path, progress);
    } else if (ext == ".3ds") {
        result = Load3DS(path, progress);
    } else {
        result.errorMessage = "Unsupported format: " + ext;
        if (progress) {
            progress->Error(result.errorMessage);
            progress->SetStatus(ImportStatus::Failed);
        }
        return result;
    }

    if (!result.success) {
        if (progress) progress->SetStatus(ImportStatus::Failed);
        return result;
    }

    if (progress) progress->EndStage();

    // Check for cancellation
    if (progress && progress->IsCancellationRequested()) {
        progress->MarkCancelled();
        return result;
    }

    // Process meshes
    if (progress) progress->BeginStage("process");

    // Apply scale
    float scale = settings.CalculateUnitScale();
    if (std::abs(scale - 1.0f) > 0.0001f) {
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        for (auto& mesh : result.meshes) {
            TransformMesh(mesh, scaleMatrix);
        }
        if (progress) progress->Info("Applied scale: " + std::to_string(scale));
    }

    // Coordinate system conversion
    if (settings.swapYZ) {
        ConvertCoordinateSystem(result, true, settings.flipWindingOrder);
        if (progress) progress->Info("Converted coordinate system (Y-Z swap)");
    }

    // Generate normals if needed
    for (auto& mesh : result.meshes) {
        if (settings.generateNormals) {
            GenerateNormals(mesh, true);
        }
        if (settings.generateTangents && !mesh.hasTangents) {
            GenerateTangents(mesh);
        }
        if (settings.mergeVertices) {
            MergeVertices(mesh, settings.mergeThreshold);
        }
        if (settings.calculateBounds) {
            CalculateBounds(mesh);
        }
    }

    // Process skeleton
    if (result.hasSkeleton && settings.importSkeleton) {
        for (auto& mesh : result.meshes) {
            if (mesh.hasBoneWeights) {
                NormalizeBoneWeights(mesh);
                LimitBonesPerVertex(mesh, settings.maxBonesPerVertex);
            }
        }
    }

    if (progress) progress->EndStage();

    // Optimization
    if (progress) progress->BeginStage("optimize");

    if (settings.optimizeMesh) {
        for (auto& mesh : result.meshes) {
            OptimizeMesh(mesh);
        }
        if (progress) progress->Info("Optimized meshes");
    }

    // LOD generation
    if (settings.generateLODs) {
        for (size_t i = 0; i < result.meshes.size(); ++i) {
            auto lods = GenerateLODs(result.meshes[i], settings.lodReductions, settings.lodDistances);
            result.lodChains.push_back(lods);
        }
        if (progress) progress->Info("Generated " + std::to_string(settings.lodReductions.size()) + " LOD levels");
    }

    if (progress) progress->EndStage();

    // Collision generation
    if (progress) progress->BeginStage("collision");

    if (settings.generateCollision) {
        for (const auto& mesh : result.meshes) {
            if (settings.convexDecomposition) {
                auto shapes = ConvexDecomposition(mesh, settings.maxConvexHulls, settings.maxVerticesPerHull);
                result.collisionShapes.insert(result.collisionShapes.end(), shapes.begin(), shapes.end());
            } else if (settings.generateSimplifiedCollision) {
                result.collisionShapes.push_back(GenerateConvexHull(mesh));
            } else {
                result.collisionShapes.push_back(GenerateTriMeshCollision(mesh, settings.collisionSimplification));
            }
        }
        if (progress) progress->Info("Generated " + std::to_string(result.collisionShapes.size()) + " collision shapes");
    }

    if (progress) progress->EndStage();

    // Finalize
    if (progress) progress->BeginStage("output");

    CalculateModelBounds(result);

    // Calculate statistics
    result.totalVertices = 0;
    result.totalTriangles = 0;
    for (const auto& mesh : result.meshes) {
        result.totalVertices += mesh.vertexCount;
        result.totalTriangles += mesh.triangleCount;
    }
    result.totalBones = static_cast<uint32_t>(result.bones.size());
    result.totalMaterials = static_cast<uint32_t>(result.materials.size());

    // Set output path
    if (settings.outputPath.empty()) {
        result.outputPath = path + ".nova";
    } else {
        result.outputPath = settings.outputPath;
    }

    if (progress) progress->EndStage();

    // Success
    result.success = true;
    if (progress) {
        if (!result.warnings.empty()) {
            progress->SetStatus(ImportStatus::CompletedWithWarnings);
        } else {
            progress->SetStatus(ImportStatus::Completed);
        }
        progress->StopTiming();
    }

    return result;
}

ImportedModel ModelImporter::Import(const std::string& path) {
    ModelImportSettings settings;
    return Import(path, settings);
}

// ============================================================================
// Batch Import
// ============================================================================

std::vector<ImportedModel> ModelImporter::ImportBatch(
    const std::vector<std::string>& paths,
    const ModelImportSettings& settings,
    ImportProgressTracker* tracker) {

    std::vector<ImportedModel> results;
    results.reserve(paths.size());

    for (const auto& path : paths) {
        ImportProgress* progress = tracker ? tracker->AddImport(path) : nullptr;
        results.push_back(Import(path, settings, progress));
    }

    return results;
}

// ============================================================================
// OBJ Loading
// ============================================================================

ImportedModel ModelImporter::LoadOBJ(const std::string& path, ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;

    OBJData objData = ParseOBJ(path);

    if (objData.positions.empty()) {
        result.errorMessage = "Failed to parse OBJ file";
        return result;
    }

    // Load materials if MTL file exists
    std::string mtlPath = fs::path(path).replace_extension(".mtl").string();
    if (fs::exists(mtlPath)) {
        result.materials = ParseMTL(mtlPath);
    }

    // Create mesh from OBJ data
    ImportedMesh mesh;
    mesh.name = fs::path(path).stem().string();

    // Build vertices from face indices
    std::unordered_map<std::string, uint32_t> vertexCache;

    for (const auto& face : objData.faces) {
        auto [posIdx, texIdx, normIdx] = face;

        // Create unique key for vertex
        std::string key = std::to_string(posIdx) + "/" +
                          std::to_string(texIdx) + "/" +
                          std::to_string(normIdx);

        auto it = vertexCache.find(key);
        if (it != vertexCache.end()) {
            mesh.indices.push_back(it->second);
        } else {
            ImportedVertex vertex;

            if (posIdx >= 0 && posIdx < static_cast<int>(objData.positions.size())) {
                vertex.position = objData.positions[posIdx];
            }
            if (texIdx >= 0 && texIdx < static_cast<int>(objData.texCoords.size())) {
                vertex.texCoord = objData.texCoords[texIdx];
            }
            if (normIdx >= 0 && normIdx < static_cast<int>(objData.normals.size())) {
                vertex.normal = objData.normals[normIdx];
            }

            uint32_t newIndex = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(vertex);
            mesh.indices.push_back(newIndex);
            vertexCache[key] = newIndex;
        }
    }

    mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    mesh.triangleCount = static_cast<uint32_t>(mesh.indices.size() / 3);

    CalculateBounds(mesh);
    result.meshes.push_back(mesh);

    result.success = true;
    return result;
}

ModelImporter::OBJData ModelImporter::ParseOBJ(const std::string& path) {
    OBJData data;

    std::ifstream file(path);
    if (!file.is_open()) {
        return data;
    }

    std::string line;
    std::string currentMaterial;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            data.positions.push_back(pos);
        }
        else if (prefix == "vn") {
            glm::vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            data.normals.push_back(norm);
        }
        else if (prefix == "vt") {
            glm::vec2 tex;
            iss >> tex.x >> tex.y;
            data.texCoords.push_back(tex);
        }
        else if (prefix == "f") {
            // Parse face - supports v, v/vt, v/vt/vn, v//vn
            std::string vertexStr;
            std::vector<std::tuple<int, int, int>> faceVerts;

            while (iss >> vertexStr) {
                int posIdx = -1, texIdx = -1, normIdx = -1;

                // Find slashes
                size_t slash1 = vertexStr.find('/');
                size_t slash2 = vertexStr.find('/', slash1 + 1);

                if (slash1 == std::string::npos) {
                    // Just position index
                    posIdx = std::stoi(vertexStr) - 1;
                } else if (slash2 == std::string::npos) {
                    // Position and texture
                    posIdx = std::stoi(vertexStr.substr(0, slash1)) - 1;
                    if (slash1 + 1 < vertexStr.length()) {
                        texIdx = std::stoi(vertexStr.substr(slash1 + 1)) - 1;
                    }
                } else {
                    // Position, texture, normal
                    posIdx = std::stoi(vertexStr.substr(0, slash1)) - 1;
                    if (slash2 - slash1 > 1) {
                        texIdx = std::stoi(vertexStr.substr(slash1 + 1, slash2 - slash1 - 1)) - 1;
                    }
                    normIdx = std::stoi(vertexStr.substr(slash2 + 1)) - 1;
                }

                faceVerts.push_back({posIdx, texIdx, normIdx});
            }

            // Triangulate face (fan triangulation)
            for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                data.faces.push_back(faceVerts[0]);
                data.faces.push_back(faceVerts[i]);
                data.faces.push_back(faceVerts[i + 1]);
            }
        }
        else if (prefix == "usemtl") {
            iss >> currentMaterial;
            data.materialGroups.push_back({currentMaterial, static_cast<int>(data.faces.size())});
        }
    }

    return data;
}

std::vector<ImportedMaterial> ModelImporter::ParseMTL(const std::string& path) {
    std::vector<ImportedMaterial> materials;

    std::ifstream file(path);
    if (!file.is_open()) {
        return materials;
    }

    ImportedMaterial* currentMat = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "newmtl") {
            materials.push_back(ImportedMaterial{});
            currentMat = &materials.back();
            iss >> currentMat->name;
        }
        else if (currentMat) {
            if (prefix == "Kd") {
                iss >> currentMat->diffuseColor.r >> currentMat->diffuseColor.g >> currentMat->diffuseColor.b;
            }
            else if (prefix == "Ks") {
                iss >> currentMat->specularColor.r >> currentMat->specularColor.g >> currentMat->specularColor.b;
            }
            else if (prefix == "Ke") {
                iss >> currentMat->emissiveColor.r >> currentMat->emissiveColor.g >> currentMat->emissiveColor.b;
            }
            else if (prefix == "Ns") {
                iss >> currentMat->shininess;
            }
            else if (prefix == "d" || prefix == "Tr") {
                iss >> currentMat->opacity;
                if (prefix == "Tr") currentMat->opacity = 1.0f - currentMat->opacity;
            }
            else if (prefix == "map_Kd") {
                MaterialTexture tex;
                iss >> tex.path;
                tex.type = "diffuse";
                currentMat->textures.push_back(tex);
            }
            else if (prefix == "map_Bump" || prefix == "bump" || prefix == "map_Kn") {
                MaterialTexture tex;
                iss >> tex.path;
                tex.type = "normal";
                currentMat->textures.push_back(tex);
            }
            else if (prefix == "map_Ks") {
                MaterialTexture tex;
                iss >> tex.path;
                tex.type = "specular";
                currentMat->textures.push_back(tex);
            }
        }
    }

    return materials;
}

// ============================================================================
// GLTF Loading
// ============================================================================

ImportedModel ModelImporter::LoadGLTF(const std::string& path, ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file";
        return result;
    }

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool isBinary = (ext == ".glb");

    if (isBinary) {
        // GLB format
        uint32_t magic, version, length;
        file.read(reinterpret_cast<char*>(&magic), 4);
        file.read(reinterpret_cast<char*>(&version), 4);
        file.read(reinterpret_cast<char*>(&length), 4);

        if (magic != 0x46546C67) {  // "glTF"
            result.errorMessage = "Invalid GLB magic number";
            return result;
        }

        // Read JSON chunk
        uint32_t jsonLength, jsonType;
        file.read(reinterpret_cast<char*>(&jsonLength), 4);
        file.read(reinterpret_cast<char*>(&jsonType), 4);

        std::string json(jsonLength, '\0');
        file.read(json.data(), jsonLength);

        // Would parse JSON here in full implementation
        // For now, create placeholder mesh
    } else {
        // Regular GLTF (JSON)
        std::string json((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        // Would parse JSON here
    }

    // Create placeholder mesh for demonstration
    ImportedMesh mesh;
    mesh.name = fs::path(path).stem().string();

    // Create a simple triangle as placeholder
    mesh.vertices.resize(3);
    mesh.vertices[0].position = {0.0f, 0.0f, 0.0f};
    mesh.vertices[1].position = {1.0f, 0.0f, 0.0f};
    mesh.vertices[2].position = {0.5f, 1.0f, 0.0f};
    mesh.indices = {0, 1, 2};
    mesh.vertexCount = 3;
    mesh.triangleCount = 1;

    result.meshes.push_back(mesh);
    result.success = true;
    return result;
}

// ============================================================================
// FBX Loading (Simplified)
// ============================================================================

ImportedModel ModelImporter::LoadFBX(const std::string& path, ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Check FBX header
    char header[23];
    file.read(header, 23);

    if (std::string(header, 20) != "Kaydara FBX Binary  ") {
        result.errorMessage = "Invalid FBX header";
        return result;
    }

    // FBX parsing is complex - would use FBX SDK or custom parser
    // Create placeholder for demonstration
    ImportedMesh mesh;
    mesh.name = fs::path(path).stem().string();
    mesh.vertices.resize(4);
    mesh.vertices[0].position = {-0.5f, -0.5f, 0.0f};
    mesh.vertices[1].position = {0.5f, -0.5f, 0.0f};
    mesh.vertices[2].position = {0.5f, 0.5f, 0.0f};
    mesh.vertices[3].position = {-0.5f, 0.5f, 0.0f};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.vertexCount = 4;
    mesh.triangleCount = 2;

    result.meshes.push_back(mesh);
    result.success = true;

    if (progress) progress->Warning("FBX import uses simplified parser");

    return result;
}

// ============================================================================
// DAE (Collada) Loading
// ============================================================================

ImportedModel ModelImporter::LoadDAE(const std::string& path, ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Would use XML parser for Collada
    // Simplified placeholder
    ImportedMesh mesh;
    mesh.name = fs::path(path).stem().string();
    result.meshes.push_back(mesh);
    result.success = true;

    if (progress) progress->Warning("DAE import uses simplified parser");

    return result;
}

// ============================================================================
// 3DS Loading
// ============================================================================

ImportedModel ModelImporter::Load3DS(const std::string& path, ImportProgress* progress) {
    ImportedModel result;
    result.sourcePath = path;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Check 3DS header
    uint16_t mainChunkId;
    file.read(reinterpret_cast<char*>(&mainChunkId), 2);

    if (mainChunkId != 0x4D4D) {
        result.errorMessage = "Invalid 3DS header";
        return result;
    }

    // 3DS chunk-based parsing
    // Simplified placeholder
    ImportedMesh mesh;
    mesh.name = fs::path(path).stem().string();
    result.meshes.push_back(mesh);
    result.success = true;

    if (progress) progress->Warning("3DS import uses simplified parser");

    return result;
}

// ============================================================================
// Mesh Processing
// ============================================================================

void ModelImporter::OptimizeMesh(ImportedMesh& mesh) {
    OptimizeVertexCache(mesh);
    OptimizeOverdraw(mesh);
}

void ModelImporter::OptimizeVertexCache(ImportedMesh& mesh) {
    // Tom Forsyth's vertex cache optimization (simplified)
    // The goal is to reorder indices for better GPU cache utilization

    if (mesh.indices.size() < 3) return;

    std::vector<uint32_t> optimizedIndices;
    optimizedIndices.reserve(mesh.indices.size());

    // Simple FIFO cache simulation
    const size_t cacheSize = 32;
    std::vector<uint32_t> cache;
    std::vector<bool> usedTriangles(mesh.indices.size() / 3, false);

    auto isInCache = [&cache](uint32_t idx) {
        return std::find(cache.begin(), cache.end(), idx) != cache.end();
    };

    auto addToCache = [&cache, cacheSize](uint32_t idx) {
        auto it = std::find(cache.begin(), cache.end(), idx);
        if (it != cache.end()) {
            cache.erase(it);
        }
        cache.push_back(idx);
        if (cache.size() > cacheSize) {
            cache.erase(cache.begin());
        }
    };

    // Greedy triangle selection based on cache score
    for (size_t i = 0; i < mesh.indices.size() / 3; ++i) {
        int bestTriangle = -1;
        int bestScore = -1;

        for (size_t t = 0; t < mesh.indices.size() / 3; ++t) {
            if (usedTriangles[t]) continue;

            int score = 0;
            for (int j = 0; j < 3; ++j) {
                if (isInCache(mesh.indices[t * 3 + j])) {
                    score += 1;
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestTriangle = static_cast<int>(t);
            }
        }

        if (bestTriangle >= 0) {
            usedTriangles[bestTriangle] = true;
            for (int j = 0; j < 3; ++j) {
                uint32_t idx = mesh.indices[bestTriangle * 3 + j];
                optimizedIndices.push_back(idx);
                addToCache(idx);
            }
        }
    }

    mesh.indices = optimizedIndices;
}

void ModelImporter::OptimizeOverdraw(ImportedMesh& mesh) {
    // Sort triangles by average depth (simplified)
    // Full implementation would use cluster-based sorting

    struct Triangle {
        uint32_t indices[3];
        float avgZ;
    };

    std::vector<Triangle> triangles(mesh.indices.size() / 3);

    for (size_t i = 0; i < triangles.size(); ++i) {
        triangles[i].indices[0] = mesh.indices[i * 3];
        triangles[i].indices[1] = mesh.indices[i * 3 + 1];
        triangles[i].indices[2] = mesh.indices[i * 3 + 2];

        triangles[i].avgZ = (mesh.vertices[triangles[i].indices[0]].position.z +
                             mesh.vertices[triangles[i].indices[1]].position.z +
                             mesh.vertices[triangles[i].indices[2]].position.z) / 3.0f;
    }

    // Sort front-to-back
    std::sort(triangles.begin(), triangles.end(),
              [](const Triangle& a, const Triangle& b) { return a.avgZ < b.avgZ; });

    // Rebuild index buffer
    for (size_t i = 0; i < triangles.size(); ++i) {
        mesh.indices[i * 3] = triangles[i].indices[0];
        mesh.indices[i * 3 + 1] = triangles[i].indices[1];
        mesh.indices[i * 3 + 2] = triangles[i].indices[2];
    }
}

void ModelImporter::CalculateBounds(ImportedMesh& mesh) {
    if (mesh.vertices.empty()) return;

    mesh.boundsMin = mesh.vertices[0].position;
    mesh.boundsMax = mesh.vertices[0].position;

    for (const auto& v : mesh.vertices) {
        mesh.boundsMin = glm::min(mesh.boundsMin, v.position);
        mesh.boundsMax = glm::max(mesh.boundsMax, v.position);
    }

    mesh.boundsCenter = (mesh.boundsMin + mesh.boundsMax) * 0.5f;
    mesh.boundsSphereRadius = glm::length(mesh.boundsMax - mesh.boundsCenter);
}

void ModelImporter::CalculateModelBounds(ImportedModel& model) {
    if (model.meshes.empty()) return;

    model.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    model.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& mesh : model.meshes) {
        model.boundsMin = glm::min(model.boundsMin, mesh.boundsMin);
        model.boundsMax = glm::max(model.boundsMax, mesh.boundsMax);
    }
}

void ModelImporter::GenerateNormals(ImportedMesh& mesh, bool smooth) {
    if (mesh.indices.empty()) return;

    // Reset normals
    for (auto& v : mesh.vertices) {
        v.normal = glm::vec3(0.0f);
    }

    // Calculate face normals and accumulate
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[i]].position;
        const auto& v1 = mesh.vertices[mesh.indices[i + 1]].position;
        const auto& v2 = mesh.vertices[mesh.indices[i + 2]].position;

        glm::vec3 faceNormal = CalculateFaceNormal(v0, v1, v2);

        if (smooth) {
            mesh.vertices[mesh.indices[i]].normal += faceNormal;
            mesh.vertices[mesh.indices[i + 1]].normal += faceNormal;
            mesh.vertices[mesh.indices[i + 2]].normal += faceNormal;
        } else {
            // Flat shading - set face normal directly
            mesh.vertices[mesh.indices[i]].normal = faceNormal;
            mesh.vertices[mesh.indices[i + 1]].normal = faceNormal;
            mesh.vertices[mesh.indices[i + 2]].normal = faceNormal;
        }
    }

    // Normalize
    if (smooth) {
        for (auto& v : mesh.vertices) {
            float len = glm::length(v.normal);
            if (len > 0.0001f) {
                v.normal /= len;
            }
        }
    }
}

void ModelImporter::GenerateTangents(ImportedMesh& mesh) {
    if (mesh.indices.empty()) return;

    // Reset tangents
    for (auto& v : mesh.vertices) {
        v.tangent = glm::vec3(0.0f);
        v.bitangent = glm::vec3(0.0f);
    }

    // Calculate tangents per triangle
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        auto& v0 = mesh.vertices[mesh.indices[i]];
        auto& v1 = mesh.vertices[mesh.indices[i + 1]];
        auto& v2 = mesh.vertices[mesh.indices[i + 2]];

        glm::vec3 tangent, bitangent;
        CalculateTangentBitangent(v0.position, v1.position, v2.position,
                                   v0.texCoord, v1.texCoord, v2.texCoord,
                                   tangent, bitangent);

        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;

        v0.bitangent += bitangent;
        v1.bitangent += bitangent;
        v2.bitangent += bitangent;
    }

    // Normalize and orthogonalize
    for (auto& v : mesh.vertices) {
        // Gram-Schmidt orthogonalize
        v.tangent = glm::normalize(v.tangent - v.normal * glm::dot(v.normal, v.tangent));

        // Calculate handedness
        if (glm::dot(glm::cross(v.normal, v.tangent), v.bitangent) < 0.0f) {
            v.tangent = -v.tangent;
        }

        v.bitangent = glm::cross(v.normal, v.tangent);
    }

    mesh.hasTangents = true;
}

void ModelImporter::MergeVertices(ImportedMesh& mesh, float threshold) {
    std::unordered_map<ImportedVertex, uint32_t, VertexHash, VertexEqual> vertexMap;
    std::vector<ImportedVertex> newVertices;
    std::vector<uint32_t> indexRemap(mesh.vertices.size());

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        auto it = vertexMap.find(mesh.vertices[i]);
        if (it != vertexMap.end()) {
            indexRemap[i] = it->second;
        } else {
            uint32_t newIndex = static_cast<uint32_t>(newVertices.size());
            newVertices.push_back(mesh.vertices[i]);
            vertexMap[mesh.vertices[i]] = newIndex;
            indexRemap[i] = newIndex;
        }
    }

    // Remap indices
    for (auto& idx : mesh.indices) {
        idx = indexRemap[idx];
    }

    mesh.vertices = newVertices;
    mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
}

void ModelImporter::TransformMesh(ImportedMesh& mesh, const glm::mat4& transform) {
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    for (auto& v : mesh.vertices) {
        v.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
        v.normal = glm::normalize(normalMatrix * v.normal);
        v.tangent = glm::normalize(normalMatrix * v.tangent);
        v.bitangent = glm::normalize(normalMatrix * v.bitangent);
    }

    CalculateBounds(mesh);
}

// ============================================================================
// LOD Generation
// ============================================================================

std::vector<LODMesh> ModelImporter::GenerateLODs(const ImportedMesh& mesh,
                                                   const std::vector<float>& reductions,
                                                   const std::vector<float>& distances) {
    std::vector<LODMesh> lods;

    // LOD 0 is the original mesh
    LODMesh lod0;
    lod0.mesh = mesh;
    lod0.screenSize = 1.0f;
    lod0.distance = 0.0f;
    lod0.reductionRatio = 1.0f;
    lods.push_back(lod0);

    // Generate additional LODs
    for (size_t i = 0; i < reductions.size(); ++i) {
        LODMesh lod;
        lod.mesh = SimplifyMesh(mesh, reductions[i]);
        lod.reductionRatio = reductions[i];
        lod.distance = i < distances.size() ? distances[i] : (i + 1) * 10.0f;
        lod.screenSize = CalculateScreenSize(lod.distance, mesh.boundsSphereRadius, 60.0f);
        lods.push_back(lod);
    }

    return lods;
}

ImportedMesh ModelImporter::SimplifyMesh(const ImportedMesh& mesh, float targetRatio) {
    ImportedMesh simplified = mesh;

    size_t targetTriangles = static_cast<size_t>(mesh.triangleCount * targetRatio);
    if (targetTriangles < 1) targetTriangles = 1;

    // Simplified edge collapse using quadric error metrics
    std::vector<QuadricError> quadrics(mesh.vertices.size());
    ComputeQuadrics(mesh, quadrics);

    // Priority queue for edge collapse
    struct EdgeCollapse {
        size_t v1, v2;
        float error;
        glm::vec3 optimalPos;

        bool operator>(const EdgeCollapse& other) const {
            return error > other.error;
        }
    };

    std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, std::greater<EdgeCollapse>> collapseQueue;

    // Initialize edge collapse candidates
    std::set<std::pair<size_t, size_t>> edges;
    for (size_t i = 0; i < simplified.indices.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            size_t v1 = simplified.indices[i + j];
            size_t v2 = simplified.indices[i + (j + 1) % 3];
            if (v1 > v2) std::swap(v1, v2);
            edges.insert({v1, v2});
        }
    }

    for (const auto& [v1, v2] : edges) {
        EdgeCollapse ec;
        ec.v1 = v1;
        ec.v2 = v2;
        ec.error = ComputeEdgeCollapseError(quadrics[v1], quadrics[v2],
                                             simplified.vertices[v1].position,
                                             simplified.vertices[v2].position,
                                             ec.optimalPos);
        collapseQueue.push(ec);
    }

    // Collapse edges until target is reached
    std::vector<bool> deletedVertices(simplified.vertices.size(), false);
    std::vector<size_t> vertexRemap(simplified.vertices.size());
    std::iota(vertexRemap.begin(), vertexRemap.end(), 0);

    while (simplified.triangleCount > targetTriangles && !collapseQueue.empty()) {
        EdgeCollapse ec = collapseQueue.top();
        collapseQueue.pop();

        // Find actual vertices after remapping
        size_t actualV1 = vertexRemap[ec.v1];
        size_t actualV2 = vertexRemap[ec.v2];

        while (actualV1 != vertexRemap[actualV1]) actualV1 = vertexRemap[actualV1];
        while (actualV2 != vertexRemap[actualV2]) actualV2 = vertexRemap[actualV2];

        if (actualV1 == actualV2 || deletedVertices[actualV1] || deletedVertices[actualV2]) {
            continue;
        }

        // Collapse v2 into v1
        simplified.vertices[actualV1].position = ec.optimalPos;
        deletedVertices[actualV2] = true;
        vertexRemap[actualV2] = actualV1;

        // Update indices
        for (auto& idx : simplified.indices) {
            if (idx == actualV2) idx = static_cast<uint32_t>(actualV1);
        }

        // Remove degenerate triangles
        std::vector<uint32_t> newIndices;
        for (size_t i = 0; i < simplified.indices.size(); i += 3) {
            uint32_t i0 = simplified.indices[i];
            uint32_t i1 = simplified.indices[i + 1];
            uint32_t i2 = simplified.indices[i + 2];
            if (i0 != i1 && i1 != i2 && i2 != i0) {
                newIndices.push_back(i0);
                newIndices.push_back(i1);
                newIndices.push_back(i2);
            }
        }
        simplified.indices = newIndices;
        simplified.triangleCount = static_cast<uint32_t>(simplified.indices.size() / 3);
    }

    // Compact vertices
    std::vector<ImportedVertex> compactVertices;
    std::vector<uint32_t> compactRemap(simplified.vertices.size(), UINT32_MAX);

    for (size_t i = 0; i < simplified.vertices.size(); ++i) {
        if (!deletedVertices[i]) {
            compactRemap[i] = static_cast<uint32_t>(compactVertices.size());
            compactVertices.push_back(simplified.vertices[i]);
        }
    }

    for (auto& idx : simplified.indices) {
        idx = compactRemap[idx];
    }

    simplified.vertices = compactVertices;
    simplified.vertexCount = static_cast<uint32_t>(simplified.vertices.size());

    return simplified;
}

float ModelImporter::CalculateScreenSize(float distance, float boundsSphereRadius, float fov) {
    float screenHeight = 2.0f * distance * std::tan(glm::radians(fov) * 0.5f);
    return (boundsSphereRadius * 2.0f) / screenHeight;
}

void ModelImporter::ComputeQuadrics(const ImportedMesh& mesh, std::vector<QuadricError>& quadrics) {
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[i]].position;
        const auto& v1 = mesh.vertices[mesh.indices[i + 1]].position;
        const auto& v2 = mesh.vertices[mesh.indices[i + 2]].position;

        glm::vec3 n = CalculateFaceNormal(v0, v1, v2);
        float d = -glm::dot(n, v0);

        // Construct fundamental quadric for plane
        glm::mat4 q = glm::mat4(0.0f);
        q[0][0] = n.x * n.x; q[0][1] = n.x * n.y; q[0][2] = n.x * n.z; q[0][3] = n.x * d;
        q[1][0] = n.y * n.x; q[1][1] = n.y * n.y; q[1][2] = n.y * n.z; q[1][3] = n.y * d;
        q[2][0] = n.z * n.x; q[2][1] = n.z * n.y; q[2][2] = n.z * n.z; q[2][3] = n.z * d;
        q[3][0] = d * n.x;   q[3][1] = d * n.y;   q[3][2] = d * n.z;   q[3][3] = d * d;

        quadrics[mesh.indices[i]].quadric += q;
        quadrics[mesh.indices[i + 1]].quadric += q;
        quadrics[mesh.indices[i + 2]].quadric += q;
    }
}

float ModelImporter::ComputeEdgeCollapseError(const QuadricError& q1, const QuadricError& q2,
                                               const glm::vec3& v1, const glm::vec3& v2,
                                               glm::vec3& optimalPos) {
    glm::mat4 q = q1.quadric + q2.quadric;

    // Try to find optimal position
    // Simplified: use midpoint
    optimalPos = (v1 + v2) * 0.5f;

    // Compute error at optimal position
    glm::vec4 v(optimalPos, 1.0f);
    return glm::dot(v, q * v);
}

// ============================================================================
// Skeleton Processing
// ============================================================================

std::unique_ptr<Skeleton> ModelImporter::BuildSkeleton(const std::vector<ImportedBone>& bones,
                                                        const glm::mat4& globalInverse) {
    auto skeleton = std::make_unique<Skeleton>();
    skeleton->SetGlobalInverseTransform(globalInverse);

    for (const auto& bone : bones) {
        Bone b;
        b.name = bone.name;
        b.parentIndex = bone.parentIndex;
        b.offsetMatrix = bone.offsetMatrix;
        b.localTransform = bone.localTransform;
        skeleton->AddBone(b);
    }

    return skeleton;
}

void ModelImporter::NormalizeBoneWeights(ImportedMesh& mesh) {
    for (auto& v : mesh.vertices) {
        float sum = v.boneWeights.x + v.boneWeights.y + v.boneWeights.z + v.boneWeights.w;
        if (sum > 0.0001f) {
            v.boneWeights /= sum;
        }
    }
}

void ModelImporter::LimitBonesPerVertex(ImportedMesh& mesh, int maxBones) {
    for (auto& v : mesh.vertices) {
        // Sort bone weights
        std::array<std::pair<int, float>, 4> boneData = {{
            {v.boneIds.x, v.boneWeights.x},
            {v.boneIds.y, v.boneWeights.y},
            {v.boneIds.z, v.boneWeights.z},
            {v.boneIds.w, v.boneWeights.w}
        }};

        std::sort(boneData.begin(), boneData.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Keep only maxBones
        for (int i = maxBones; i < 4; ++i) {
            boneData[i].first = -1;
            boneData[i].second = 0.0f;
        }

        v.boneIds = glm::ivec4(boneData[0].first, boneData[1].first,
                                boneData[2].first, boneData[3].first);
        v.boneWeights = glm::vec4(boneData[0].second, boneData[1].second,
                                   boneData[2].second, boneData[3].second);
    }

    NormalizeBoneWeights(mesh);
}

// ============================================================================
// Collision Generation
// ============================================================================

CollisionShape ModelImporter::GenerateConvexHull(const ImportedMesh& mesh) {
    CollisionShape shape;
    shape.type = CollisionShapeType::ConvexHull;
    shape.name = mesh.name + "_convex";

    // Gift wrapping algorithm for convex hull (simplified)
    shape.vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices) {
        shape.vertices.push_back(v.position);
    }

    // Would use quickhull or similar in production
    // For now, just copy all vertices

    return shape;
}

CollisionShape ModelImporter::GenerateBoxCollision(const ImportedMesh& mesh) {
    CollisionShape shape;
    shape.type = CollisionShapeType::Box;
    shape.name = mesh.name + "_box";
    shape.center = mesh.boundsCenter;
    shape.halfExtents = (mesh.boundsMax - mesh.boundsMin) * 0.5f;
    return shape;
}

CollisionShape ModelImporter::GenerateSphereCollision(const ImportedMesh& mesh) {
    CollisionShape shape;
    shape.type = CollisionShapeType::Sphere;
    shape.name = mesh.name + "_sphere";
    shape.center = mesh.boundsCenter;
    shape.radius = mesh.boundsSphereRadius;
    return shape;
}

std::vector<CollisionShape> ModelImporter::ConvexDecomposition(const ImportedMesh& mesh,
                                                                 int maxHulls,
                                                                 int maxVerticesPerHull) {
    std::vector<CollisionShape> shapes;

    // Simplified: create one convex hull
    // Full implementation would use V-HACD or similar
    shapes.push_back(GenerateConvexHull(mesh));

    return shapes;
}

CollisionShape ModelImporter::GenerateTriMeshCollision(const ImportedMesh& mesh, float simplification) {
    CollisionShape shape;
    shape.type = CollisionShapeType::TriangleMesh;
    shape.name = mesh.name + "_trimesh";

    if (simplification < 1.0f) {
        ImportedMesh simplified = SimplifyMesh(mesh, simplification);
        for (const auto& v : simplified.vertices) {
            shape.vertices.push_back(v.position);
        }
        shape.indices = simplified.indices;
    } else {
        for (const auto& v : mesh.vertices) {
            shape.vertices.push_back(v.position);
        }
        shape.indices = mesh.indices;
    }

    return shape;
}

// ============================================================================
// Coordinate System
// ============================================================================

void ModelImporter::ConvertCoordinateSystem(ImportedModel& model, bool swapYZ, bool flipWindingOrder) {
    for (auto& mesh : model.meshes) {
        for (auto& v : mesh.vertices) {
            if (swapYZ) {
                std::swap(v.position.y, v.position.z);
                std::swap(v.normal.y, v.normal.z);
                std::swap(v.tangent.y, v.tangent.z);
                std::swap(v.bitangent.y, v.bitangent.z);
            }
        }

        if (flipWindingOrder) {
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
            }
        }

        CalculateBounds(mesh);
    }

    CalculateModelBounds(model);
}

void ModelImporter::NormalizeScale(ImportedModel& model, float targetSize) {
    glm::vec3 size = model.boundsMax - model.boundsMin;
    float maxDim = std::max({size.x, size.y, size.z});

    if (maxDim < 0.0001f) return;

    float scale = targetSize / maxDim;
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    for (auto& mesh : model.meshes) {
        TransformMesh(mesh, scaleMatrix);
    }

    CalculateModelBounds(model);
}

// ============================================================================
// File Format Support
// ============================================================================

bool ModelImporter::IsFormatSupported(const std::string& extension) {
    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::vector<std::string> supported = {
        ".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds"
    };

    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

std::vector<std::string> ModelImporter::GetSupportedExtensions() {
    return {".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds"};
}

std::string ModelImporter::DetectFormat(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    char header[32];
    file.read(header, 32);

    // FBX binary
    if (std::string(header, 20) == "Kaydara FBX Binary  ") {
        return "FBX";
    }

    // GLB
    if (header[0] == 'g' && header[1] == 'l' && header[2] == 'T' && header[3] == 'F') {
        return "GLB";
    }

    // 3DS
    uint16_t magic = *reinterpret_cast<uint16_t*>(header);
    if (magic == 0x4D4D) {
        return "3DS";
    }

    return fs::path(path).extension().string();
}

// ============================================================================
// Output
// ============================================================================

bool ModelImporter::SaveEngineFormat(const ImportedModel& model, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    struct Header {
        char magic[4] = {'N', 'M', 'D', 'L'};
        uint32_t version = 1;
        uint32_t meshCount;
        uint32_t materialCount;
        uint32_t boneCount;
    };

    Header header;
    header.meshCount = static_cast<uint32_t>(model.meshes.size());
    header.materialCount = static_cast<uint32_t>(model.materials.size());
    header.boneCount = static_cast<uint32_t>(model.bones.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write mesh data (simplified)
    for (const auto& mesh : model.meshes) {
        uint32_t vertCount = static_cast<uint32_t>(mesh.vertices.size());
        uint32_t idxCount = static_cast<uint32_t>(mesh.indices.size());

        file.write(reinterpret_cast<const char*>(&vertCount), 4);
        file.write(reinterpret_cast<const char*>(&idxCount), 4);
        file.write(reinterpret_cast<const char*>(mesh.vertices.data()),
                   vertCount * sizeof(ImportedVertex));
        file.write(reinterpret_cast<const char*>(mesh.indices.data()),
                   idxCount * sizeof(uint32_t));
    }

    return true;
}

bool ModelImporter::ExportOBJ(const ImportedMesh& mesh, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "# Exported by Nova Engine\n";
    file << "# Vertices: " << mesh.vertices.size() << "\n";
    file << "# Triangles: " << mesh.indices.size() / 3 << "\n\n";

    // Vertices
    for (const auto& v : mesh.vertices) {
        file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
    }

    file << "\n";

    // Texture coordinates
    for (const auto& v : mesh.vertices) {
        file << "vt " << v.texCoord.x << " " << v.texCoord.y << "\n";
    }

    file << "\n";

    // Normals
    for (const auto& v : mesh.vertices) {
        file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
    }

    file << "\n";

    // Faces
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        file << "f "
             << mesh.indices[i] + 1 << "/" << mesh.indices[i] + 1 << "/" << mesh.indices[i] + 1 << " "
             << mesh.indices[i + 1] + 1 << "/" << mesh.indices[i + 1] + 1 << "/" << mesh.indices[i + 1] + 1 << " "
             << mesh.indices[i + 2] + 1 << "/" << mesh.indices[i + 2] + 1 << "/" << mesh.indices[i + 2] + 1 << "\n";
    }

    return true;
}

std::string ModelImporter::ExportMetadata(const ImportedModel& model) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"sourcePath\": \"" << model.sourcePath << "\",\n";
    ss << "  \"meshCount\": " << model.meshes.size() << ",\n";
    ss << "  \"materialCount\": " << model.materials.size() << ",\n";
    ss << "  \"totalVertices\": " << model.totalVertices << ",\n";
    ss << "  \"totalTriangles\": " << model.totalTriangles << ",\n";
    ss << "  \"totalBones\": " << model.totalBones << ",\n";
    ss << "  \"hasSkeleton\": " << (model.hasSkeleton ? "true" : "false") << ",\n";
    ss << "  \"boundsMin\": [" << model.boundsMin.x << ", " << model.boundsMin.y << ", " << model.boundsMin.z << "],\n";
    ss << "  \"boundsMax\": [" << model.boundsMax.x << ", " << model.boundsMax.y << ", " << model.boundsMax.z << "]\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

glm::vec3 CalculateFaceNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    return glm::normalize(glm::cross(edge1, edge2));
}

void CalculateTangentBitangent(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                                const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                glm::vec3& tangent, glm::vec3& bitangent) {
    glm::vec3 edge1 = p1 - p0;
    glm::vec3 edge2 = p2 - p0;
    glm::vec2 deltaUV1 = uv1 - uv0;
    glm::vec2 deltaUV2 = uv2 - uv0;

    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
}

size_t VertexHash::operator()(const ImportedVertex& v) const {
    size_t h = 0;
    auto hashFloat = [](float f) { return std::hash<float>{}(f); };

    h ^= hashFloat(v.position.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.position.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.position.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.normal.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.normal.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.normal.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.texCoord.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hashFloat(v.texCoord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);

    return h;
}

bool VertexEqual::operator()(const ImportedVertex& a, const ImportedVertex& b) const {
    const float eps = 0.0001f;
    return glm::length(a.position - b.position) < eps &&
           glm::length(a.normal - b.normal) < eps &&
           glm::length(a.texCoord - b.texCoord) < eps;
}

} // namespace Nova
