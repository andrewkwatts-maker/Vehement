#include "graphics/Material.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"

#include <glad/gl.h>

namespace Nova {

Material::Material() = default;

Material::Material(std::shared_ptr<Shader> shader)
    : m_shader(std::move(shader))
{}

void Material::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = std::move(shader);
}

void Material::Bind() const {
    if (!m_shader) return;

    m_shader->Bind();

    // Bind textures
    for (const auto& [name, binding] : m_textures) {
        if (binding.texture) {
            binding.texture->Bind(binding.slot);
            m_shader->SetInt(name, binding.slot);
        }
    }

    // Set PBR uniforms
    m_shader->SetVec3("u_Material.albedo", m_albedo);
    m_shader->SetFloat("u_Material.metallic", m_metallic);
    m_shader->SetFloat("u_Material.roughness", m_roughness);
    m_shader->SetFloat("u_Material.ao", m_ao);
    m_shader->SetVec3("u_Material.emissive", m_emissive);

    // Set custom uniforms
    for (const auto& [name, value] : m_floats) {
        m_shader->SetFloat(name, value);
    }
    for (const auto& [name, value] : m_ints) {
        m_shader->SetInt(name, value);
    }
    for (const auto& [name, value] : m_vec2s) {
        m_shader->SetVec2(name, value);
    }
    for (const auto& [name, value] : m_vec3s) {
        m_shader->SetVec3(name, value);
    }
    for (const auto& [name, value] : m_vec4s) {
        m_shader->SetVec4(name, value);
    }
    for (const auto& [name, value] : m_mat3s) {
        m_shader->SetMat3(name, value);
    }
    for (const auto& [name, value] : m_mat4s) {
        m_shader->SetMat4(name, value);
    }

    // Handle two-sided rendering
    if (m_twoSided) {
        glDisable(GL_CULL_FACE);
    }

    // Handle transparency
    if (m_transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void Material::Unbind() const {
    if (m_twoSided) {
        glEnable(GL_CULL_FACE);
    }
    if (m_transparent) {
        glDisable(GL_BLEND);
    }
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture> texture, uint32_t slot) {
    m_textures[name] = {std::move(texture), slot};
}

void Material::SetAlbedoMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_AlbedoMap", std::move(texture), 0);
}

void Material::SetNormalMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_NormalMap", std::move(texture), 1);
}

void Material::SetMetallicMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_MetallicMap", std::move(texture), 2);
}

void Material::SetRoughnessMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_RoughnessMap", std::move(texture), 3);
}

void Material::SetAOMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_AOMap", std::move(texture), 4);
}

void Material::SetEmissiveMap(std::shared_ptr<Texture> texture) {
    SetTexture("u_EmissiveMap", std::move(texture), 5);
}

void Material::SetAlbedo(const glm::vec3& color) {
    m_albedo = color;
}

void Material::SetMetallic(float value) {
    m_metallic = value;
}

void Material::SetRoughness(float value) {
    m_roughness = value;
}

void Material::SetAO(float value) {
    m_ao = value;
}

void Material::SetEmissive(const glm::vec3& color) {
    m_emissive = color;
}

void Material::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
}

void Material::SetInt(const std::string& name, int value) {
    m_ints[name] = value;
}

void Material::SetVec2(const std::string& name, const glm::vec2& value) {
    m_vec2s[name] = value;
}

void Material::SetVec3(const std::string& name, const glm::vec3& value) {
    m_vec3s[name] = value;
}

void Material::SetVec4(const std::string& name, const glm::vec4& value) {
    m_vec4s[name] = value;
}

void Material::SetMat3(const std::string& name, const glm::mat3& value) {
    m_mat3s[name] = value;
}

void Material::SetMat4(const std::string& name, const glm::mat4& value) {
    m_mat4s[name] = value;
}

} // namespace Nova
