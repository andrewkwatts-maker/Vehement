#include "graphics/Shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cassert>

namespace Nova {

// Debug validation for uniform setters - warns if shader not bound
#ifdef NDEBUG
    #define VALIDATE_SHADER_BOUND() ((void)0)
#else
    #define VALIDATE_SHADER_BOUND() \
        do { \
            if (!IsBound()) { \
                spdlog::warn("Setting uniform on unbound shader (program {})", m_programID); \
            } \
        } while(0)
#endif

Shader::Shader() = default;

Shader::~Shader() {
    Cleanup();
}

Shader::Shader(Shader&& other) noexcept
    : m_programID(other.m_programID)
    , m_vertexPath(std::move(other.m_vertexPath))
    , m_fragmentPath(std::move(other.m_fragmentPath))
    , m_geometryPath(std::move(other.m_geometryPath))
    , m_uniformCache(std::move(other.m_uniformCache))
{
    other.m_programID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_programID = other.m_programID;
        m_vertexPath = std::move(other.m_vertexPath);
        m_fragmentPath = std::move(other.m_fragmentPath);
        m_geometryPath = std::move(other.m_geometryPath);
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_programID = 0;
    }
    return *this;
}

bool Shader::Load(const std::string& vertexPath, const std::string& fragmentPath,
                  const std::string& geometryPath) {
    m_vertexPath = vertexPath;
    m_fragmentPath = fragmentPath;
    m_geometryPath = geometryPath;

    m_vertexSource = ReadFile(vertexPath);
    if (m_vertexSource.empty()) {
        spdlog::error("Failed to read vertex shader: {}", vertexPath);
        return false;
    }

    m_fragmentSource = ReadFile(fragmentPath);
    if (m_fragmentSource.empty()) {
        spdlog::error("Failed to read fragment shader: {}", fragmentPath);
        return false;
    }

    if (!geometryPath.empty()) {
        m_geometrySource = ReadFile(geometryPath);
        if (m_geometrySource.empty()) {
            spdlog::error("Failed to read geometry shader: {}", geometryPath);
            return false;
        }
    }

    // Preprocess includes
    std::string basePath = std::filesystem::path(vertexPath).parent_path().string();
    m_vertexSource = PreprocessShader(m_vertexSource, basePath);
    m_fragmentSource = PreprocessShader(m_fragmentSource, basePath);
    if (!m_geometrySource.empty()) {
        m_geometrySource = PreprocessShader(m_geometrySource, basePath);
    }

    return LoadFromSource(m_vertexSource, m_fragmentSource, m_geometrySource);
}

bool Shader::LoadFromSource(const std::string& vertexSource, const std::string& fragmentSource,
                            const std::string& geometrySource) {
    Cleanup();
    m_uniformCache.clear();

    // Compile vertex shader
    uint32_t vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return false;
    }

    // Compile fragment shader
    uint32_t fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    // Compile geometry shader (optional)
    uint32_t geometryShader = 0;
    if (!geometrySource.empty()) {
        geometryShader = CompileShader(GL_GEOMETRY_SHADER, geometrySource);
        if (geometryShader == 0) {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }
    }

    // Link program
    if (!LinkProgram(vertexShader, fragmentShader, geometryShader)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        if (geometryShader) glDeleteShader(geometryShader);
        return false;
    }

    // Cleanup shaders (they're linked now)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader) glDeleteShader(geometryShader);

    return true;
}

bool Shader::LoadComputeShader(const std::string& computeSource) {
    Cleanup();
    m_uniformCache.clear();

    // Compile compute shader
    uint32_t computeShader = CompileShader(GL_COMPUTE_SHADER, computeSource);
    if (computeShader == 0) {
        return false;
    }

    // Create and link program
    m_programID = glCreateProgram();
    glAttachShader(m_programID, computeShader);
    glLinkProgram(m_programID);

    int success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
        spdlog::error("Compute shader program linking failed: {}", infoLog);
        glDeleteProgram(m_programID);
        m_programID = 0;
        glDeleteShader(computeShader);
        return false;
    }

    glDeleteShader(computeShader);
    return true;
}

bool Shader::Reload() {
    if (m_vertexPath.empty() || m_fragmentPath.empty()) {
        spdlog::warn("Cannot reload shader without file paths");
        return false;
    }

    spdlog::info("Reloading shader: {}", m_vertexPath);
    return Load(m_vertexPath, m_fragmentPath, m_geometryPath);
}

// Track currently bound shader for debug validation
static uint32_t s_currentlyBoundShader = 0;

void Shader::Bind() const {
    glUseProgram(m_programID);
    s_currentlyBoundShader = m_programID;
}

void Shader::Unbind() {
    glUseProgram(0);
    s_currentlyBoundShader = 0;
}

bool Shader::IsBound() const {
    return s_currentlyBoundShader == m_programID && m_programID != 0;
}

uint32_t Shader::CompileShader(uint32_t type, const std::string& source) {
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);

        const char* typeStr = type == GL_VERTEX_SHADER ? "VERTEX" :
                              type == GL_FRAGMENT_SHADER ? "FRAGMENT" : "GEOMETRY";
        spdlog::error("{} shader compilation failed:\n{}", typeStr, infoLog);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::LinkProgram(uint32_t vertexShader, uint32_t fragmentShader, uint32_t geometryShader) {
    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertexShader);
    glAttachShader(m_programID, fragmentShader);
    if (geometryShader) {
        glAttachShader(m_programID, geometryShader);
    }
    glLinkProgram(m_programID);

    int success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_programID, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("Shader program linking failed:\n{}", infoLog);

        glDeleteProgram(m_programID);
        m_programID = 0;
        return false;
    }

    return true;
}

void Shader::Cleanup() {
    if (m_programID) {
        glDeleteProgram(m_programID);
        m_programID = 0;
    }
}

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string Shader::PreprocessShader(const std::string& source, const std::string& basePath) {
    std::string result;
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        // Handle #include directives
        if (line.find("#include") != std::string::npos) {
            size_t start = line.find('"');
            size_t end = line.rfind('"');
            if (start != std::string::npos && end != std::string::npos && start < end) {
                std::string includePath = line.substr(start + 1, end - start - 1);
                std::string fullPath = basePath + "/" + includePath;
                std::string includeSource = ReadFile(fullPath);
                if (!includeSource.empty()) {
                    result += PreprocessShader(includeSource, basePath);
                } else {
                    spdlog::warn("Failed to include shader: {}", fullPath);
                }
            }
        } else {
            result += line + "\n";
        }
    }

    return result;
}

int Shader::GetUniformLocation(const std::string& name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }

    int location = glGetUniformLocation(m_programID, name.c_str());
    m_uniformCache[name] = location;
    return location;
}

void Shader::SetBool(const std::string& name, bool value) {
    VALIDATE_SHADER_BOUND();
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void Shader::SetInt(const std::string& name, int value) {
    VALIDATE_SHADER_BOUND();
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetUInt(const std::string& name, uint32_t value) {
    VALIDATE_SHADER_BOUND();
    glUniform1ui(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    VALIDATE_SHADER_BOUND();
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) {
    VALIDATE_SHADER_BOUND();
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) {
    VALIDATE_SHADER_BOUND();
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) {
    VALIDATE_SHADER_BOUND();
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetIVec2(const std::string& name, const glm::ivec2& value) {
    VALIDATE_SHADER_BOUND();
    glUniform2iv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetIVec3(const std::string& name, const glm::ivec3& value) {
    VALIDATE_SHADER_BOUND();
    glUniform3iv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetIVec4(const std::string& name, const glm::ivec4& value) {
    VALIDATE_SHADER_BOUND();
    glUniform4iv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetMat3(const std::string& name, const glm::mat3& value) {
    VALIDATE_SHADER_BOUND();
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) {
    VALIDATE_SHADER_BOUND();
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetFloatArray(const std::string& name, const float* values, int count) {
    VALIDATE_SHADER_BOUND();
    glUniform1fv(GetUniformLocation(name), count, values);
}

void Shader::SetIntArray(const std::string& name, const int* values, int count) {
    VALIDATE_SHADER_BOUND();
    glUniform1iv(GetUniformLocation(name), count, values);
}

void Shader::SetVec3Array(const std::string& name, const glm::vec3* values, int count) {
    VALIDATE_SHADER_BOUND();
    glUniform3fv(GetUniformLocation(name), count, glm::value_ptr(values[0]));
}

void Shader::SetMat4Array(const std::string& name, const glm::mat4* values, int count) {
    VALIDATE_SHADER_BOUND();
    glUniformMatrix4fv(GetUniformLocation(name), count, GL_FALSE, glm::value_ptr(values[0]));
}

} // namespace Nova
