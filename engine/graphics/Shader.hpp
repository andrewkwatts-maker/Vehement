#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief OpenGL shader program wrapper
 *
 * Handles shader compilation, linking, and uniform management.
 */
class Shader {
public:
    Shader();
    ~Shader();

    // Delete copy, allow move
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /**
     * @brief Load shader from files
     * @param vertexPath Path to vertex shader
     * @param fragmentPath Path to fragment shader
     * @param geometryPath Optional path to geometry shader
     * @return true if loaded successfully
     */
    bool Load(const std::string& vertexPath, const std::string& fragmentPath,
              const std::string& geometryPath = "");

    /**
     * @brief Load shader from source strings
     */
    bool LoadFromSource(const std::string& vertexSource, const std::string& fragmentSource,
                        const std::string& geometrySource = "");

    /**
     * @brief Reload the shader from disk
     */
    bool Reload();

    /**
     * @brief Bind this shader for rendering
     */
    void Bind() const;

    /**
     * @brief Unbind any shader
     */
    static void Unbind();

    /**
     * @brief Get the OpenGL program ID
     */
    uint32_t GetID() const { return m_programID; }

    /**
     * @brief Check if shader is valid
     */
    bool IsValid() const { return m_programID != 0; }

    // Uniform setters
    void SetBool(const std::string& name, bool value);
    void SetInt(const std::string& name, int value);
    void SetUInt(const std::string& name, uint32_t value);
    void SetFloat(const std::string& name, float value);

    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);

    void SetIVec2(const std::string& name, const glm::ivec2& value);
    void SetIVec3(const std::string& name, const glm::ivec3& value);
    void SetIVec4(const std::string& name, const glm::ivec4& value);

    void SetMat3(const std::string& name, const glm::mat3& value);
    void SetMat4(const std::string& name, const glm::mat4& value);

    void SetFloatArray(const std::string& name, const float* values, int count);
    void SetIntArray(const std::string& name, const int* values, int count);
    void SetVec3Array(const std::string& name, const glm::vec3* values, int count);
    void SetMat4Array(const std::string& name, const glm::mat4* values, int count);

    /**
     * @brief Get uniform location (cached)
     */
    int GetUniformLocation(const std::string& name) const;

    /**
     * @brief Check if this shader is currently bound
     */
    bool IsBound() const;

    /**
     * @brief Get the file paths used to load this shader
     */
    const std::string& GetVertexPath() const { return m_vertexPath; }
    const std::string& GetFragmentPath() const { return m_fragmentPath; }
    const std::string& GetGeometryPath() const { return m_geometryPath; }

private:
    uint32_t CompileShader(uint32_t type, const std::string& source);
    bool LinkProgram(uint32_t vertexShader, uint32_t fragmentShader, uint32_t geometryShader = 0);
    void Cleanup();

    static std::string ReadFile(const std::string& path);
    static std::string PreprocessShader(const std::string& source, const std::string& basePath);

    uint32_t m_programID = 0;
    std::string m_vertexPath;
    std::string m_fragmentPath;
    std::string m_geometryPath;
    std::string m_vertexSource;
    std::string m_fragmentSource;
    std::string m_geometrySource;

    mutable std::unordered_map<std::string, int> m_uniformCache;
};

} // namespace Nova
