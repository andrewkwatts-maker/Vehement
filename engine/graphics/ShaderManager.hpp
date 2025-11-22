#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Nova {

class Shader;

/**
 * @brief Shader resource manager
 */
class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager() = default;

    /**
     * @brief Load a shader from files
     */
    std::shared_ptr<Shader> Load(const std::string& name,
                                  const std::string& vertexPath,
                                  const std::string& fragmentPath,
                                  const std::string& geometryPath = "");

    /**
     * @brief Get a previously loaded shader
     */
    std::shared_ptr<Shader> Get(const std::string& name);

    /**
     * @brief Check if a shader exists
     */
    bool Has(const std::string& name) const;

    /**
     * @brief Reload a shader from disk
     */
    bool Reload(const std::string& name);

    /**
     * @brief Reload all shaders
     */
    void ReloadAll();

    /**
     * @brief Remove a shader
     */
    void Remove(const std::string& name);

    /**
     * @brief Clear all shaders
     */
    void Clear();

private:
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
};

} // namespace Nova
