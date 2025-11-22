#include "graphics/ShaderManager.hpp"
#include "graphics/Shader.hpp"
#include <spdlog/spdlog.h>

namespace Nova {

std::shared_ptr<Shader> ShaderManager::Load(const std::string& name,
                                             const std::string& vertexPath,
                                             const std::string& fragmentPath,
                                             const std::string& geometryPath) {
    // Check if already loaded
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->Load(vertexPath, fragmentPath, geometryPath)) {
        spdlog::error("Failed to load shader: {}", name);
        return nullptr;
    }

    m_shaders[name] = shader;
    return shader;
}

std::shared_ptr<Shader> ShaderManager::Get(const std::string& name) {
    auto it = m_shaders.find(name);
    return it != m_shaders.end() ? it->second : nullptr;
}

bool ShaderManager::Has(const std::string& name) const {
    return m_shaders.find(name) != m_shaders.end();
}

bool ShaderManager::Reload(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it == m_shaders.end()) {
        return false;
    }
    return it->second->Reload();
}

void ShaderManager::ReloadAll() {
    for (auto& [name, shader] : m_shaders) {
        shader->Reload();
    }
}

void ShaderManager::Remove(const std::string& name) {
    m_shaders.erase(name);
}

void ShaderManager::Clear() {
    m_shaders.clear();
}

} // namespace Nova
