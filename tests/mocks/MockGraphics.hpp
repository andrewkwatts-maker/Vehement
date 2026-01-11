/**
 * @file MockGraphics.hpp
 * @brief Mock implementations for graphics-related dependencies used in GI testing
 */

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace Nova {
namespace Test {

// =============================================================================
// MockShader
// =============================================================================

/**
 * @brief Mock shader for testing graphics systems without GPU
 */
class MockShader {
public:
    MOCK_METHOD(bool, Load, (const std::string& vertexPath, const std::string& fragmentPath), ());
    MOCK_METHOD(bool, LoadCompute, (const std::string& computePath), ());
    MOCK_METHOD(void, Use, (), ());
    MOCK_METHOD(void, Dispatch, (int x, int y, int z), ());

    // Uniform setters
    MOCK_METHOD(void, SetInt, (const std::string& name, int value), ());
    MOCK_METHOD(void, SetFloat, (const std::string& name, float value), ());
    MOCK_METHOD(void, SetVec2, (const std::string& name, const glm::vec2& value), ());
    MOCK_METHOD(void, SetVec3, (const std::string& name, const glm::vec3& value), ());
    MOCK_METHOD(void, SetVec4, (const std::string& name, const glm::vec4& value), ());
    MOCK_METHOD(void, SetMat3, (const std::string& name, const glm::mat3& value), ());
    MOCK_METHOD(void, SetMat4, (const std::string& name, const glm::mat4& value), ());

    // State tracking for verification
    bool IsLoaded() const { return m_loaded; }
    void SetLoaded(bool loaded) { m_loaded = loaded; }

    uint32_t GetProgramID() const { return m_programId; }
    void SetProgramID(uint32_t id) { m_programId = id; }

private:
    bool m_loaded = false;
    uint32_t m_programId = 0;
};

// =============================================================================
// MockTexture
// =============================================================================

/**
 * @brief Mock texture for testing without GPU texture allocation
 */
class MockTexture {
public:
    MOCK_METHOD(bool, Load, (const std::string& path), ());
    MOCK_METHOD(bool, Create, (int width, int height, int format), ());
    MOCK_METHOD(void, Bind, (int unit), ());
    MOCK_METHOD(void, Unbind, (), ());
    MOCK_METHOD(void, SetData, (const void* data, size_t size), ());
    MOCK_METHOD(void, GetData, (void* data, size_t size), (const));

    // Properties
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetFormat() const { return m_format; }
    uint32_t GetTextureID() const { return m_textureId; }

    void SetWidth(int width) { m_width = width; }
    void SetHeight(int height) { m_height = height; }
    void SetFormat(int format) { m_format = format; }
    void SetTextureID(uint32_t id) { m_textureId = id; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_format = 0;
    uint32_t m_textureId = 0;
};

// =============================================================================
// MockTexture3D
// =============================================================================

/**
 * @brief Mock 3D texture for testing volume rendering and GI cascades
 */
class MockTexture3D {
public:
    MOCK_METHOD(bool, Create, (int width, int height, int depth, int format), ());
    MOCK_METHOD(void, Bind, (int unit), ());
    MOCK_METHOD(void, SetData, (const void* data, size_t size), ());
    MOCK_METHOD(void, Clear, (), ());

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    uint32_t GetTextureID() const { return m_textureId; }

    void SetDimensions(int width, int height, int depth) {
        m_width = width;
        m_height = height;
        m_depth = depth;
    }
    void SetTextureID(uint32_t id) { m_textureId = id; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    uint32_t m_textureId = 0;
};

// =============================================================================
// MockFramebuffer
// =============================================================================

/**
 * @brief Mock framebuffer for testing render targets
 */
class MockFramebuffer {
public:
    MOCK_METHOD(bool, Create, (int width, int height), ());
    MOCK_METHOD(void, Bind, (), ());
    MOCK_METHOD(void, Unbind, (), ());
    MOCK_METHOD(void, AttachColorTexture, (std::shared_ptr<MockTexture> texture, int attachment), ());
    MOCK_METHOD(void, AttachDepthTexture, (std::shared_ptr<MockTexture> texture), ());
    MOCK_METHOD(bool, IsComplete, (), (const));
    MOCK_METHOD(void, Clear, (float r, float g, float b, float a), ());

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    uint32_t GetFramebufferID() const { return m_framebufferId; }

    void SetWidth(int width) { m_width = width; }
    void SetHeight(int height) { m_height = height; }
    void SetFramebufferID(uint32_t id) { m_framebufferId = id; }

private:
    int m_width = 0;
    int m_height = 0;
    uint32_t m_framebufferId = 0;
};

// =============================================================================
// MockBuffer
// =============================================================================

/**
 * @brief Mock GPU buffer for testing SSBO, UBO, etc.
 */
class MockBuffer {
public:
    enum class Type {
        Vertex,
        Index,
        Uniform,
        ShaderStorage
    };

    MOCK_METHOD(bool, Create, (Type type, size_t size), ());
    MOCK_METHOD(void, Bind, (), ());
    MOCK_METHOD(void, BindBase, (int bindingPoint), ());
    MOCK_METHOD(void, SetData, (const void* data, size_t size, size_t offset), ());
    MOCK_METHOD(void, GetData, (void* data, size_t size, size_t offset), (const));

    Type GetType() const { return m_type; }
    size_t GetSize() const { return m_size; }
    uint32_t GetBufferID() const { return m_bufferId; }

    void SetType(Type type) { m_type = type; }
    void SetSize(size_t size) { m_size = size; }
    void SetBufferID(uint32_t id) { m_bufferId = id; }

private:
    Type m_type = Type::Vertex;
    size_t m_size = 0;
    uint32_t m_bufferId = 0;
};

// =============================================================================
// MockDebugDraw
// =============================================================================

/**
 * @brief Mock debug draw interface for testing visualization
 */
class MockDebugDraw {
public:
    MOCK_METHOD(void, AddLine, (const glm::vec3& start, const glm::vec3& end, const glm::vec4& color), ());
    MOCK_METHOD(void, AddSphere, (const glm::vec3& center, float radius, const glm::vec4& color), ());
    MOCK_METHOD(void, AddBox, (const glm::vec3& min, const glm::vec3& max, const glm::vec4& color), ());
    MOCK_METHOD(void, AddFrustum, (const glm::mat4& viewProjection, const glm::vec4& color), ());
    MOCK_METHOD(void, Flush, (), ());
    MOCK_METHOD(void, Clear, (), ());

    // Tracking
    struct DrawCall {
        enum Type { Line, Sphere, Box, Frustum } type;
        glm::vec3 position;
        glm::vec4 color;
    };

    const std::vector<DrawCall>& GetDrawCalls() const { return m_drawCalls; }
    void TrackDrawCall(DrawCall::Type type, const glm::vec3& pos, const glm::vec4& color) {
        m_drawCalls.push_back({type, pos, color});
    }
    void Reset() { m_drawCalls.clear(); }

private:
    std::vector<DrawCall> m_drawCalls;
};

// =============================================================================
// MockGPUTimer
// =============================================================================

/**
 * @brief Mock GPU timer for testing performance measurement
 */
class MockGPUTimer {
public:
    MOCK_METHOD(void, Begin, (), ());
    MOCK_METHOD(void, End, (), ());
    MOCK_METHOD(float, GetElapsedMs, (), (const));
    MOCK_METHOD(bool, IsResultAvailable, (), (const));

    void SetElapsedMs(float ms) { m_elapsedMs = ms; }

private:
    float m_elapsedMs = 0.0f;
};

// =============================================================================
// Graphics Resource Factory
// =============================================================================

/**
 * @brief Factory for creating mock graphics resources
 */
class MockGraphicsFactory {
public:
    static MockGraphicsFactory& Instance() {
        static MockGraphicsFactory instance;
        return instance;
    }

    std::shared_ptr<MockShader> CreateShader() {
        auto shader = std::make_shared<::testing::NiceMock<MockShader>>();
        m_shaders.push_back(shader);
        return shader;
    }

    std::shared_ptr<MockTexture> CreateTexture() {
        auto texture = std::make_shared<::testing::NiceMock<MockTexture>>();
        m_textures.push_back(texture);
        return texture;
    }

    std::shared_ptr<MockTexture3D> CreateTexture3D() {
        auto texture = std::make_shared<::testing::NiceMock<MockTexture3D>>();
        m_textures3D.push_back(texture);
        return texture;
    }

    std::shared_ptr<MockFramebuffer> CreateFramebuffer() {
        auto fb = std::make_shared<::testing::NiceMock<MockFramebuffer>>();
        m_framebuffers.push_back(fb);
        return fb;
    }

    std::shared_ptr<MockBuffer> CreateBuffer() {
        auto buffer = std::make_shared<::testing::NiceMock<MockBuffer>>();
        m_buffers.push_back(buffer);
        return buffer;
    }

    void Reset() {
        m_shaders.clear();
        m_textures.clear();
        m_textures3D.clear();
        m_framebuffers.clear();
        m_buffers.clear();
    }

    // Accessors for verification
    const std::vector<std::shared_ptr<MockShader>>& GetShaders() const { return m_shaders; }
    const std::vector<std::shared_ptr<MockTexture>>& GetTextures() const { return m_textures; }

private:
    MockGraphicsFactory() = default;

    std::vector<std::shared_ptr<MockShader>> m_shaders;
    std::vector<std::shared_ptr<MockTexture>> m_textures;
    std::vector<std::shared_ptr<MockTexture3D>> m_textures3D;
    std::vector<std::shared_ptr<MockFramebuffer>> m_framebuffers;
    std::vector<std::shared_ptr<MockBuffer>> m_buffers;
};

// =============================================================================
// Convenience Macros
// =============================================================================

#define MOCK_GRAPHICS() Nova::Test::MockGraphicsFactory::Instance()

// =============================================================================
// Test Helpers for Graphics
// =============================================================================

/**
 * @brief Compare two images (as vectors of vec3)
 */
inline float CompareImages(const std::vector<glm::vec3>& a,
                           const std::vector<glm::vec3>& b) {
    if (a.size() != b.size()) return -1.0f;
    if (a.empty()) return 1.0f;

    float totalDiff = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        totalDiff += glm::length(a[i] - b[i]);
    }
    return totalDiff / a.size();
}

/**
 * @brief Calculate PSNR between two images
 */
inline float CalculatePSNR(const std::vector<glm::vec3>& reference,
                           const std::vector<glm::vec3>& test) {
    if (reference.size() != test.size() || reference.empty()) {
        return 0.0f;
    }

    float mse = 0.0f;
    for (size_t i = 0; i < reference.size(); ++i) {
        glm::vec3 diff = reference[i] - test[i];
        mse += glm::dot(diff, diff);
    }
    mse /= (reference.size() * 3.0f);

    if (mse < 1e-10f) return 100.0f; // Perfect match

    return 10.0f * std::log10(1.0f / mse);
}

/**
 * @brief Generate a test pattern image
 */
inline std::vector<glm::vec3> GenerateTestPattern(int width, int height) {
    std::vector<glm::vec3> image(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;

            // Checkerboard with gradient
            bool check = ((x / 8) + (y / 8)) % 2 == 0;
            float intensity = check ? 0.8f : 0.2f;

            image[idx] = glm::vec3(u * intensity, v * intensity, (1.0f - u) * intensity);
        }
    }

    return image;
}

/**
 * @brief Check if an image contains any NaN or Inf values
 */
inline bool ImageIsValid(const std::vector<glm::vec3>& image) {
    for (const auto& pixel : image) {
        if (std::isnan(pixel.r) || std::isnan(pixel.g) || std::isnan(pixel.b)) {
            return false;
        }
        if (std::isinf(pixel.r) || std::isinf(pixel.g) || std::isinf(pixel.b)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if an image has all pixels in valid range [0, 1]
 */
inline bool ImageInRange(const std::vector<glm::vec3>& image) {
    for (const auto& pixel : image) {
        if (pixel.r < 0.0f || pixel.r > 1.0f) return false;
        if (pixel.g < 0.0f || pixel.g > 1.0f) return false;
        if (pixel.b < 0.0f || pixel.b > 1.0f) return false;
    }
    return true;
}

} // namespace Test
} // namespace Nova
