#include "graphics/ModelLoader.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Texture.hpp"
#include "graphics/Material.hpp"
#include "animation/Skeleton.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace Nova {

std::unordered_map<std::string, std::shared_ptr<Texture>> ModelLoader::s_textureCache;

namespace {
    glm::vec3 ConvertVec3(const aiVector3D& v) {
        return glm::vec3(v.x, v.y, v.z);
    }

    // Note: ConvertMatrix helper for aiMatrix4x4 -> glm::mat4 can be added here
    // when skeleton/animation loading is implemented.

    std::shared_ptr<Texture> LoadMaterialTexture(const aiMaterial* mat, aiTextureType type,
                                                  const std::string& directory) {
        if (mat->GetTextureCount(type) == 0) {
            return nullptr;
        }

        aiString str;
        mat->GetTexture(type, 0, &str);

        std::string texturePath = directory + "/" + str.C_Str();

        // Check cache
        auto it = ModelLoader::s_textureCache.find(texturePath);
        if (it != ModelLoader::s_textureCache.end()) {
            return it->second;
        }

        // Load texture
        auto texture = std::make_shared<Texture>();
        bool sRGB = (type == aiTextureType_DIFFUSE || type == aiTextureType_EMISSIVE);

        if (texture->Load(texturePath, sRGB)) {
            ModelLoader::s_textureCache[texturePath] = texture;
            return texture;
        }

        return nullptr;
    }

    std::unique_ptr<Mesh> ProcessMesh(const aiMesh* mesh) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vertices.reserve(mesh->mNumVertices);

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex vertex;

            vertex.position = ConvertVec3(mesh->mVertices[i]);

            if (mesh->HasNormals()) {
                vertex.normal = ConvertVec3(mesh->mNormals[i]);
            }

            if (mesh->mTextureCoords[0]) {
                vertex.texCoords = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            }

            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent = ConvertVec3(mesh->mTangents[i]);
                vertex.bitangent = ConvertVec3(mesh->mBitangents[i]);
            }

            // Bone weights would be processed here for skeletal animation
            // vertex.boneIDs and vertex.boneWeights

            vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j]);
            }
        }

        auto result = std::make_unique<Mesh>();
        result->Create(vertices, indices);
        return result;
    }

    std::shared_ptr<Material> ProcessMaterial(const aiMaterial* mat, const std::string& directory) {
        auto material = std::make_shared<Material>();

        // Load textures
        auto diffuse = LoadMaterialTexture(mat, aiTextureType_DIFFUSE, directory);
        if (diffuse) material->SetAlbedoMap(diffuse);

        auto normal = LoadMaterialTexture(mat, aiTextureType_NORMALS, directory);
        if (!normal) normal = LoadMaterialTexture(mat, aiTextureType_HEIGHT, directory);
        if (normal) material->SetNormalMap(normal);

        auto metallic = LoadMaterialTexture(mat, aiTextureType_METALNESS, directory);
        if (metallic) material->SetMetallicMap(metallic);

        auto roughness = LoadMaterialTexture(mat, aiTextureType_DIFFUSE_ROUGHNESS, directory);
        if (roughness) material->SetRoughnessMap(roughness);

        auto ao = LoadMaterialTexture(mat, aiTextureType_AMBIENT_OCCLUSION, directory);
        if (ao) material->SetAOMap(ao);

        auto emissive = LoadMaterialTexture(mat, aiTextureType_EMISSIVE, directory);
        if (emissive) material->SetEmissiveMap(emissive);

        // Get material properties
        aiColor3D color(1.0f, 1.0f, 1.0f);
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            material->SetAlbedo(glm::vec3(color.r, color.g, color.b));
        }

        float metalValue = 0.0f;
        if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metalValue) == AI_SUCCESS) {
            material->SetMetallic(metalValue);
        }

        float roughValue = 0.5f;
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughValue) == AI_SUCCESS) {
            material->SetRoughness(roughValue);
        }

        return material;
    }

    void ProcessNode(const aiNode* node, const aiScene* scene, Model& model,
                     const std::string& directory, bool loadMaterials) {
        // Process meshes in this node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            model.meshes.push_back(ProcessMesh(mesh));

            // Add material
            if (loadMaterials && mesh->mMaterialIndex < scene->mNumMaterials) {
                const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
                model.materials.push_back(ProcessMaterial(mat, directory));
            }
        }

        // Process children
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            ProcessNode(node->mChildren[i], scene, model, directory, loadMaterials);
        }
    }
}

std::unique_ptr<Model> ModelLoader::Load(const std::string& path,
                                         bool loadMaterials,
                                         bool loadAnimations) {
    spdlog::info("Loading model: {}", path);

    Assimp::Importer importer;

    unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_ImproveCacheLocality |
        aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFile(path, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        spdlog::error("Failed to load model: {}", importer.GetErrorString());
        return nullptr;
    }

    auto model = std::make_unique<Model>();
    std::string directory = std::filesystem::path(path).parent_path().string();

    ProcessNode(scene->mRootNode, scene, *model, directory, loadMaterials);

    // Calculate bounds
    model->boundsMin = glm::vec3(std::numeric_limits<float>::max());
    model->boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& mesh : model->meshes) {
        model->boundsMin = glm::min(model->boundsMin, mesh->GetBoundsMin());
        model->boundsMax = glm::max(model->boundsMax, mesh->GetBoundsMax());
    }

    spdlog::info("Loaded model with {} meshes and {} materials",
                 model->meshes.size(), model->materials.size());

    return model;
}

std::vector<std::string> ModelLoader::GetSupportedExtensions() {
    return {
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".blend",
        ".3ds", ".ase", ".ifc", ".xgl", ".zgl", ".ply",
        ".dxf", ".lwo", ".lws", ".lxo", ".stl", ".x",
        ".ac", ".ms3d", ".cob", ".scn"
    };
}

bool ModelLoader::IsSupported(const std::string& extension) {
    auto supported = GetSupportedExtensions();
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

void ModelLoader::ClearCache() {
    s_textureCache.clear();
}

} // namespace Nova
