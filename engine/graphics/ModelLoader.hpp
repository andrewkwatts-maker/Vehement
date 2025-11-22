#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

class Mesh;
class Texture;
class Material;
class Skeleton;

/**
 * @brief Loaded model data
 */
struct Model {
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::unique_ptr<Skeleton> skeleton;

    glm::vec3 boundsMin{0};
    glm::vec3 boundsMax{0};

    bool HasSkeleton() const { return skeleton != nullptr; }
};

/**
 * @brief Model loading system using Assimp
 *
 * Full-featured model loading with support for FBX, OBJ, GLTF, DAE,
 * and many other formats. Handles meshes, materials, and textures.
 */
class ModelLoader {
public:
    /**
     * @brief Load a model from file
     * @param path Path to the model file
     * @param loadMaterials Whether to load embedded materials
     * @param loadAnimations Whether to process animation data
     * @return Loaded model or nullptr on failure
     */
    static std::unique_ptr<Model> Load(const std::string& path,
                                       bool loadMaterials = true,
                                       bool loadAnimations = true);

    /**
     * @brief Get supported file extensions
     */
    static std::vector<std::string> GetSupportedExtensions();

    /**
     * @brief Check if a file format is supported
     */
    static bool IsSupported(const std::string& extension);

    /**
     * @brief Clear cached textures
     */
    static void ClearCache();

private:
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_textureCache;
};

} // namespace Nova
