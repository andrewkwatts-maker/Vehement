#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

class Shader;
class Texture;

/**
 * @brief Material class for rendering
 *
 * Combines a shader with textures and uniform values.
 */
class Material {
public:
    Material();
    explicit Material(std::shared_ptr<Shader> shader);
    ~Material() = default;

    /**
     * @brief Set the shader for this material
     */
    void SetShader(std::shared_ptr<Shader> shader);

    /**
     * @brief Get the shader
     */
    Shader& GetShader() const { return *m_shader; }
    std::shared_ptr<Shader> GetShaderPtr() const { return m_shader; }

    /**
     * @brief Bind the material for rendering
     */
    void Bind() const;

    /**
     * @brief Unbind the material
     */
    void Unbind() const;

    // Texture setters
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture, uint32_t slot);
    void SetAlbedoMap(std::shared_ptr<Texture> texture);
    void SetNormalMap(std::shared_ptr<Texture> texture);
    void SetMetallicMap(std::shared_ptr<Texture> texture);
    void SetRoughnessMap(std::shared_ptr<Texture> texture);
    void SetAOMap(std::shared_ptr<Texture> texture);
    void SetEmissiveMap(std::shared_ptr<Texture> texture);

    // PBR property setters
    void SetAlbedo(const glm::vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetAO(float value);
    void SetEmissive(const glm::vec3& color);

    // Generic uniform setters
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetMat3(const std::string& name, const glm::mat3& value);
    void SetMat4(const std::string& name, const glm::mat4& value);

    // Rendering options
    void SetTwoSided(bool twoSided) { m_twoSided = twoSided; }
    bool IsTwoSided() const { return m_twoSided; }

    void SetTransparent(bool transparent) { m_transparent = transparent; }
    bool IsTransparent() const { return m_transparent; }

private:
    struct TextureBinding {
        std::shared_ptr<Texture> texture;
        uint32_t slot;
    };

    std::shared_ptr<Shader> m_shader;
    std::unordered_map<std::string, TextureBinding> m_textures;

    // Cached uniform values
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, int> m_ints;
    std::unordered_map<std::string, glm::vec2> m_vec2s;
    std::unordered_map<std::string, glm::vec3> m_vec3s;
    std::unordered_map<std::string, glm::vec4> m_vec4s;
    std::unordered_map<std::string, glm::mat3> m_mat3s;
    std::unordered_map<std::string, glm::mat4> m_mat4s;

    // PBR defaults
    glm::vec3 m_albedo{1.0f};
    float m_metallic = 0.0f;
    float m_roughness = 0.5f;
    float m_ao = 1.0f;
    glm::vec3 m_emissive{0.0f};

    bool m_twoSided = false;
    bool m_transparent = false;

    // Track state changes made during Bind() for proper restoration
    mutable bool m_previousCullingState = true;
    mutable bool m_previousBlendingState = false;
};

} // namespace Nova
