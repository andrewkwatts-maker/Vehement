#include "postprocess/PostProcess.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include "core/Logger.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <random>

namespace Nova {

// ============================================================================
// Shader Sources
// ============================================================================

static const char* const FULLSCREEN_VERTEX_SHADER = R"(
#version 460 core
out vec2 v_TexCoord;
void main() {
    vec2 vertices[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    v_TexCoord = gl_Position.xy * 0.5 + 0.5;
}
)";

static const char* const COPY_FRAGMENT_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
void main() {
    FragColor = texture(u_Texture, v_TexCoord);
}
)";

static const char* const BLOOM_THRESHOLD_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform float u_Threshold;
uniform float u_SoftKnee;

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    float brightness = max(max(color.r, color.g), color.b);
    float soft = brightness - u_Threshold + u_SoftKnee;
    soft = clamp(soft, 0.0, 2.0 * u_SoftKnee);
    soft = soft * soft / (4.0 * u_SoftKnee + 0.00001);
    float contribution = max(soft, brightness - u_Threshold) / max(brightness, 0.00001);
    FragColor = vec4(color * contribution, 1.0);
}
)";

static const char* const BLOOM_DOWNSAMPLE_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform vec2 u_TexelSize;

void main() {
    // 13-tap filter for smooth downsampling
    vec3 a = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(-2, 2)).rgb;
    vec3 b = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(0, 2)).rgb;
    vec3 c = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(2, 2)).rgb;
    vec3 d = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(-2, 0)).rgb;
    vec3 e = texture(u_Texture, v_TexCoord).rgb;
    vec3 f = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(2, 0)).rgb;
    vec3 g = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(-2, -2)).rgb;
    vec3 h = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(0, -2)).rgb;
    vec3 i = texture(u_Texture, v_TexCoord + u_TexelSize * vec2(2, -2)).rgb;

    vec3 color = e * 0.25;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (texture(u_Texture, v_TexCoord + u_TexelSize * vec2(-1, 1)).rgb +
              texture(u_Texture, v_TexCoord + u_TexelSize * vec2(1, 1)).rgb +
              texture(u_Texture, v_TexCoord + u_TexelSize * vec2(-1, -1)).rgb +
              texture(u_Texture, v_TexCoord + u_TexelSize * vec2(1, -1)).rgb) * 0.125;

    FragColor = vec4(color, 1.0);
}
)";

static const char* const BLOOM_UPSAMPLE_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform sampler2D u_BloomTexture;
uniform vec2 u_TexelSize;
uniform float u_Radius;

void main() {
    // 9-tap tent filter
    vec3 bloom = vec3(0.0);
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(-1, -1) * u_TexelSize * u_Radius).rgb;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(0, -1) * u_TexelSize * u_Radius).rgb * 2.0;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(1, -1) * u_TexelSize * u_Radius).rgb;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(-1, 0) * u_TexelSize * u_Radius).rgb * 2.0;
    bloom += texture(u_BloomTexture, v_TexCoord).rgb * 4.0;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(1, 0) * u_TexelSize * u_Radius).rgb * 2.0;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(-1, 1) * u_TexelSize * u_Radius).rgb;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(0, 1) * u_TexelSize * u_Radius).rgb * 2.0;
    bloom += texture(u_BloomTexture, v_TexCoord + vec2(1, 1) * u_TexelSize * u_Radius).rgb;
    bloom /= 16.0;

    vec3 base = texture(u_Texture, v_TexCoord).rgb;
    FragColor = vec4(base + bloom, 1.0);
}
)";

static const char* const BLOOM_COMPOSITE_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_SceneTexture;
uniform sampler2D u_BloomTexture;
uniform float u_Intensity;
uniform vec3 u_Tint;

void main() {
    vec3 scene = texture(u_SceneTexture, v_TexCoord).rgb;
    vec3 bloom = texture(u_BloomTexture, v_TexCoord).rgb;
    FragColor = vec4(scene + bloom * u_Intensity * u_Tint, 1.0);
}
)";

static const char* const TONEMAPPING_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform int u_Operator;
uniform float u_Exposure;
uniform float u_Gamma;
uniform float u_WhitePoint;

vec3 reinhardToneMap(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 reinhardExtendedToneMap(vec3 color, float white) {
    vec3 numerator = color * (1.0 + (color / vec3(white * white)));
    return numerator / (1.0 + color);
}

vec3 acesToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 uncharted2ToneMap(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
    vec3 hdrColor = texture(u_Texture, v_TexCoord).rgb * u_Exposure;

    vec3 mapped;
    if (u_Operator == 0) { // None
        mapped = hdrColor;
    } else if (u_Operator == 1) { // Reinhard
        mapped = reinhardToneMap(hdrColor);
    } else if (u_Operator == 2) { // Reinhard Extended
        mapped = reinhardExtendedToneMap(hdrColor, u_WhitePoint);
    } else if (u_Operator == 3) { // ACES
        mapped = acesToneMap(hdrColor);
    } else if (u_Operator == 4) { // Uncharted 2
        float W = 11.2;
        vec3 curr = uncharted2ToneMap(hdrColor);
        vec3 whiteScale = 1.0 / uncharted2ToneMap(vec3(W));
        mapped = curr * whiteScale;
    } else { // Exposure only
        mapped = vec3(1.0) - exp(-hdrColor);
    }

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / u_Gamma));

    FragColor = vec4(mapped, 1.0);
}
)";

static const char* const COLOR_GRADING_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform sampler3D u_LUT;
uniform float u_LUTIntensity;
uniform bool u_HasLUT;
uniform float u_Contrast;
uniform float u_Saturation;
uniform float u_Brightness;
uniform float u_HueShift;
uniform vec3 u_Lift;
uniform vec3 u_Gamma;
uniform vec3 u_Gain;
uniform float u_Temperature;
uniform float u_Tint;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 applyLiftGammaGain(vec3 color) {
    vec3 luma = vec3(dot(color, vec3(0.2126, 0.7152, 0.0722)));
    vec3 shadows = u_Lift;
    vec3 midtones = u_Gamma;
    vec3 highlights = u_Gain;

    color = color * highlights;
    color = pow(color, 1.0 / midtones);
    color = color + shadows * (1.0 - color);

    return color;
}

vec3 applyTemperature(vec3 color, float temp, float tint) {
    // Simple temperature/tint adjustment
    mat3 colorMatrix = mat3(
        1.0 + temp * 0.1, 0.0, 0.0,
        tint * 0.1, 1.0, -tint * 0.1,
        0.0, 0.0, 1.0 - temp * 0.1
    );
    return colorMatrix * color;
}

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;

    // Apply brightness
    color += u_Brightness;

    // Apply contrast
    color = (color - 0.5) * u_Contrast + 0.5;

    // Apply saturation
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, u_Saturation);

    // Apply hue shift
    if (abs(u_HueShift) > 0.001) {
        vec3 hsv = rgb2hsv(color);
        hsv.x = fract(hsv.x + u_HueShift / 360.0);
        color = hsv2rgb(hsv);
    }

    // Apply lift/gamma/gain
    color = applyLiftGammaGain(color);

    // Apply temperature/tint
    if (abs(u_Temperature) > 0.001 || abs(u_Tint) > 0.001) {
        color = applyTemperature(color, u_Temperature / 100.0, u_Tint / 100.0);
    }

    // Apply LUT
    if (u_HasLUT && u_LUTIntensity > 0.0) {
        vec3 lutColor = texture(u_LUT, clamp(color, 0.0, 1.0)).rgb;
        color = mix(color, lutColor, u_LUTIntensity);
    }

    FragColor = vec4(max(color, 0.0), 1.0);
}
)";

static const char* const SSAO_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out float FragColor;
uniform sampler2D u_DepthTexture;
uniform sampler2D u_NoiseTexture;
uniform vec3 u_Samples[64];
uniform mat4 u_Projection;
uniform mat4 u_View;
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Intensity;
uniform int u_SampleCount;
uniform vec2 u_ScreenSize;
uniform vec2 u_NoiseScale;
uniform float u_Near;
uniform float u_Far;

float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * u_Near * u_Far) / (u_Far + u_Near - z * (u_Far - u_Near));
}

vec3 viewPosFromDepth(vec2 uv, float depth) {
    float z = linearizeDepth(depth);
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inverse(u_Projection) * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth = texture(u_DepthTexture, v_TexCoord).r;
    if (depth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    vec3 fragPos = viewPosFromDepth(v_TexCoord, depth);
    vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));

    vec3 randomVec = normalize(texture(u_NoiseTexture, v_TexCoord * u_NoiseScale).xyz * 2.0 - 1.0);

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < u_SampleCount; ++i) {
        vec3 samplePos = fragPos + TBN * u_Samples[i] * u_Radius;
        vec4 offset = u_Projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = linearizeDepth(texture(u_DepthTexture, offset.xy).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / abs(linearizeDepth(depth) - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(u_SampleCount)) * u_Intensity;
    FragColor = pow(occlusion, 2.0);
}
)";

static const char* const BLUR_SHADER = R"(
#version 460 core
in vec2 v_TexCoord;
out float FragColor;
uniform sampler2D u_Texture;
uniform vec2 u_Direction;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));
    float result = 0.0;

    for (int i = -2; i <= 2; ++i) {
        vec2 offset = u_Direction * float(i) * texelSize;
        result += texture(u_Texture, v_TexCoord + offset).r;
    }

    FragColor = result / 5.0;
}
)";

// ============================================================================
// BloomEffect Implementation
// ============================================================================

bool BloomEffect::Initialize() {
    m_thresholdShader = std::make_unique<Shader>();
    if (!m_thresholdShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, BLOOM_THRESHOLD_SHADER)) {
        LOG_ERROR("Failed to create bloom threshold shader");
        return false;
    }

    m_downsampleShader = std::make_unique<Shader>();
    if (!m_downsampleShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, BLOOM_DOWNSAMPLE_SHADER)) {
        LOG_ERROR("Failed to create bloom downsample shader");
        return false;
    }

    m_upsampleShader = std::make_unique<Shader>();
    if (!m_upsampleShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, BLOOM_UPSAMPLE_SHADER)) {
        LOG_ERROR("Failed to create bloom upsample shader");
        return false;
    }

    m_compositeShader = std::make_unique<Shader>();
    if (!m_compositeShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, BLOOM_COMPOSITE_SHADER)) {
        LOG_ERROR("Failed to create bloom composite shader");
        return false;
    }

    return true;
}

void BloomEffect::Shutdown() {
    for (auto fbo : m_mipFBOs) {
        if (fbo) glDeleteFramebuffers(1, &fbo);
    }
    for (auto tex : m_mipTextures) {
        if (tex) glDeleteTextures(1, &tex);
    }
    m_mipFBOs.clear();
    m_mipTextures.clear();
    m_mipSizes.clear();

    m_thresholdShader.reset();
    m_downsampleShader.reset();
    m_upsampleShader.reset();
    m_compositeShader.reset();
}

void BloomEffect::Resize(int width, int height) {
    PostProcessEffect::Resize(width, height);
    CreateMipChain();
}

void BloomEffect::CreateMipChain() {
    // Cleanup old resources
    for (auto fbo : m_mipFBOs) {
        if (fbo) glDeleteFramebuffers(1, &fbo);
    }
    for (auto tex : m_mipTextures) {
        if (tex) glDeleteTextures(1, &tex);
    }
    m_mipFBOs.clear();
    m_mipTextures.clear();
    m_mipSizes.clear();

    // Create mip chain
    int w = m_width / 2;
    int h = m_height / 2;

    for (int i = 0; i < kMaxMipLevels && w > 1 && h > 1; ++i) {
        uint32_t tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        uint32_t fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        m_mipTextures.push_back(tex);
        m_mipFBOs.push_back(fbo);
        m_mipSizes.push_back({w, h});

        w /= 2;
        h /= 2;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomEffect::Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t /*depthTexture*/) {
    if (m_mipFBOs.empty()) return;

    // 1. Threshold pass -> mip 0
    glBindFramebuffer(GL_FRAMEBUFFER, m_mipFBOs[0]);
    glViewport(0, 0, m_mipSizes[0].x, m_mipSizes[0].y);

    m_thresholdShader->Bind();
    m_thresholdShader->SetFloat("u_Threshold", m_params.threshold);
    m_thresholdShader->SetFloat("u_SoftKnee", m_params.softKnee);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 2. Downsample passes
    m_downsampleShader->Bind();
    for (size_t i = 1; i < m_mipFBOs.size(); ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_mipFBOs[i]);
        glViewport(0, 0, m_mipSizes[i].x, m_mipSizes[i].y);

        m_downsampleShader->SetVec2("u_TexelSize",
            glm::vec2(1.0f / m_mipSizes[i-1].x, 1.0f / m_mipSizes[i-1].y));
        glBindTexture(GL_TEXTURE_2D, m_mipTextures[i-1]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // 3. Upsample passes
    m_upsampleShader->Bind();
    m_upsampleShader->SetFloat("u_Radius", m_params.radius);

    for (int i = static_cast<int>(m_mipFBOs.size()) - 2; i >= 0; --i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_mipFBOs[i]);
        glViewport(0, 0, m_mipSizes[i].x, m_mipSizes[i].y);

        m_upsampleShader->SetVec2("u_TexelSize",
            glm::vec2(1.0f / m_mipSizes[i+1].x, 1.0f / m_mipSizes[i+1].y));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_mipTextures[i]);
        m_upsampleShader->SetInt("u_Texture", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_mipTextures[i+1]);
        m_upsampleShader->SetInt("u_BloomTexture", 1);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // 4. Composite
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, m_width, m_height);

    m_compositeShader->Bind();
    m_compositeShader->SetFloat("u_Intensity", m_params.intensity);
    m_compositeShader->SetVec3("u_Tint", m_params.tint);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    m_compositeShader->SetInt("u_SceneTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_mipTextures[0]);
    m_compositeShader->SetInt("u_BloomTexture", 1);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ============================================================================
// ToneMappingEffect Implementation
// ============================================================================

bool ToneMappingEffect::Initialize() {
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, TONEMAPPING_SHADER)) {
        LOG_ERROR("Failed to create tone mapping shader");
        return false;
    }
    return true;
}

void ToneMappingEffect::Shutdown() {
    m_shader.reset();
    if (m_luminanceFBO) glDeleteFramebuffers(1, &m_luminanceFBO);
    if (m_luminanceTexture) glDeleteTextures(1, &m_luminanceTexture);
}

void ToneMappingEffect::Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t /*depthTexture*/) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, m_width, m_height);

    m_shader->Bind();
    m_shader->SetInt("u_Operator", static_cast<int>(m_params.op));
    m_shader->SetFloat("u_Exposure", m_params.autoExposure ? m_currentExposure : m_params.exposure);
    m_shader->SetFloat("u_Gamma", m_params.gamma);
    m_shader->SetFloat("u_WhitePoint", m_params.whitePoint);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    m_shader->SetInt("u_Texture", 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void ToneMappingEffect::UpdateAutoExposure(uint32_t /*inputTexture*/, float /*deltaTime*/) {
    // Auto-exposure implementation using luminance histogram
    // (simplified version)
}

// ============================================================================
// ColorGradingEffect Implementation
// ============================================================================

bool ColorGradingEffect::Initialize() {
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, COLOR_GRADING_SHADER)) {
        LOG_ERROR("Failed to create color grading shader");
        return false;
    }
    return true;
}

void ColorGradingEffect::Shutdown() {
    m_shader.reset();
    m_lutTexture.reset();
}

void ColorGradingEffect::SetParams(const ColorGradingParams& params) {
    if (params.lutPath != m_params.lutPath && !params.lutPath.empty()) {
        LoadLUT(params.lutPath);
    }
    m_params = params;
}

bool ColorGradingEffect::LoadLUT(const std::string& path) {
    m_lutTexture = std::make_shared<Texture>();
    // Load as 3D texture (assuming 32x32x32 or 64x64x64 LUT)
    return m_lutTexture->Load(path);
}

void ColorGradingEffect::Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t /*depthTexture*/) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, m_width, m_height);

    m_shader->Bind();
    m_shader->SetFloat("u_Contrast", m_params.contrast);
    m_shader->SetFloat("u_Saturation", m_params.saturation);
    m_shader->SetFloat("u_Brightness", m_params.brightness);
    m_shader->SetFloat("u_HueShift", m_params.hueShift);
    m_shader->SetVec3("u_Lift", m_params.lift);
    m_shader->SetVec3("u_Gamma", m_params.gamma);
    m_shader->SetVec3("u_Gain", m_params.gain);
    m_shader->SetFloat("u_Temperature", m_params.temperature);
    m_shader->SetFloat("u_Tint", m_params.tint);
    m_shader->SetBool("u_HasLUT", m_lutTexture != nullptr);
    m_shader->SetFloat("u_LUTIntensity", m_params.lutIntensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    m_shader->SetInt("u_Texture", 0);

    if (m_lutTexture) {
        glActiveTexture(GL_TEXTURE1);
        m_lutTexture->Bind(1);
        m_shader->SetInt("u_LUT", 1);
    }

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ============================================================================
// SSAOEffect Implementation
// ============================================================================

bool SSAOEffect::Initialize() {
    m_ssaoShader = std::make_unique<Shader>();
    if (!m_ssaoShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, SSAO_SHADER)) {
        LOG_ERROR("Failed to create SSAO shader");
        return false;
    }

    m_blurShader = std::make_unique<Shader>();
    if (!m_blurShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, BLUR_SHADER)) {
        LOG_ERROR("Failed to create SSAO blur shader");
        return false;
    }

    GenerateKernel();
    GenerateNoiseTexture();

    return true;
}

void SSAOEffect::Shutdown() {
    m_ssaoShader.reset();
    m_blurShader.reset();
    if (m_ssaoFBO) glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_ssaoTexture) glDeleteTextures(1, &m_ssaoTexture);
    if (m_blurFBO) glDeleteFramebuffers(1, &m_blurFBO);
    if (m_blurTexture) glDeleteTextures(1, &m_blurTexture);
    if (m_noiseTexture) glDeleteTextures(1, &m_noiseTexture);
}

void SSAOEffect::Resize(int width, int height) {
    PostProcessEffect::Resize(width, height);

    int aoWidth = m_params.halfResolution ? width / 2 : width;
    int aoHeight = m_params.halfResolution ? height / 2 : height;

    // Create SSAO framebuffer
    if (m_ssaoFBO) glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_ssaoTexture) glDeleteTextures(1, &m_ssaoTexture);

    glGenTextures(1, &m_ssaoTexture);
    glBindTexture(GL_TEXTURE_2D, m_ssaoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aoWidth, aoHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &m_ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTexture, 0);

    // Create blur framebuffer
    if (m_blurFBO) glDeleteFramebuffers(1, &m_blurFBO);
    if (m_blurTexture) glDeleteTextures(1, &m_blurTexture);

    glGenTextures(1, &m_blurTexture);
    glBindTexture(GL_TEXTURE_2D, m_blurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aoWidth, aoHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &m_blurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blurTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAOEffect::SetParams(const AmbientOcclusionParams& params) {
    bool needsKernelUpdate = params.samples != m_params.samples;
    m_params = params;

    switch (params.quality) {
        case AmbientOcclusionParams::Quality::Low: m_params.samples = 16; break;
        case AmbientOcclusionParams::Quality::Medium: m_params.samples = 32; break;
        case AmbientOcclusionParams::Quality::High: m_params.samples = 64; break;
        case AmbientOcclusionParams::Quality::Ultra: m_params.samples = 128; break;
    }

    if (needsKernelUpdate) {
        GenerateKernel();
    }
}

void SSAOEffect::SetMatrices(const glm::mat4& view, const glm::mat4& projection) {
    m_view = view;
    m_projection = projection;
}

void SSAOEffect::GenerateNoiseTexture() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::mt19937 gen(std::random_device{}());

    std::vector<glm::vec3> noise(16);
    for (auto& n : noise) {
        n = glm::vec3(dist(gen) * 2.0f - 1.0f, dist(gen) * 2.0f - 1.0f, 0.0f);
    }

    glGenTextures(1, &m_noiseTexture);
    glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAOEffect::GenerateKernel() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::mt19937 gen(std::random_device{}());

    m_kernel.resize(m_params.samples);
    for (int i = 0; i < m_params.samples; ++i) {
        glm::vec3 sample(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            dist(gen)
        );
        sample = glm::normalize(sample) * dist(gen);

        // Scale samples to be more aligned to the center of the kernel
        float scale = static_cast<float>(i) / m_params.samples;
        scale = 0.1f + scale * scale * 0.9f;
        m_kernel[i] = sample * scale;
    }
}

void SSAOEffect::Apply(uint32_t /*inputTexture*/, uint32_t outputFBO, uint32_t depthTexture) {
    if (depthTexture == 0) return;

    int aoWidth = m_params.halfResolution ? m_width / 2 : m_width;
    int aoHeight = m_params.halfResolution ? m_height / 2 : m_height;

    // 1. SSAO pass
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glViewport(0, 0, aoWidth, aoHeight);

    m_ssaoShader->Bind();
    m_ssaoShader->SetMat4("u_Projection", m_projection);
    m_ssaoShader->SetMat4("u_View", m_view);
    m_ssaoShader->SetFloat("u_Radius", m_params.radius);
    m_ssaoShader->SetFloat("u_Bias", m_params.bias);
    m_ssaoShader->SetFloat("u_Intensity", m_params.intensity);
    m_ssaoShader->SetInt("u_SampleCount", std::min(m_params.samples, 64));
    m_ssaoShader->SetVec2("u_ScreenSize", glm::vec2(aoWidth, aoHeight));
    m_ssaoShader->SetVec2("u_NoiseScale", glm::vec2(aoWidth / 4.0f, aoHeight / 4.0f));
    m_ssaoShader->SetFloat("u_Near", 0.1f);
    m_ssaoShader->SetFloat("u_Far", 1000.0f);

    for (int i = 0; i < std::min(static_cast<int>(m_kernel.size()), 64); ++i) {
        m_ssaoShader->SetVec3("u_Samples[" + std::to_string(i) + "]", m_kernel[i]);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    m_ssaoShader->SetInt("u_DepthTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
    m_ssaoShader->SetInt("u_NoiseTexture", 1);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 2. Blur pass (horizontal)
    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO);
    m_blurShader->Bind();
    m_blurShader->SetVec2("u_Direction", glm::vec2(1.0f, 0.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_ssaoTexture);
    m_blurShader->SetInt("u_Texture", 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 3. Blur pass (vertical) + composite to output
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, m_width, m_height);
    m_blurShader->SetVec2("u_Direction", glm::vec2(0.0f, 1.0f));
    glBindTexture(GL_TEXTURE_2D, m_blurTexture);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ============================================================================
// MotionBlurEffect Implementation
// ============================================================================

bool MotionBlurEffect::Initialize() {
    // Motion blur shader implementation
    return true;
}

void MotionBlurEffect::Shutdown() {
    m_shader.reset();
}

void MotionBlurEffect::Apply(uint32_t /*inputTexture*/, uint32_t /*outputFBO*/, uint32_t /*depthTexture*/) {
    // Motion blur implementation
}

void MotionBlurEffect::SetVelocityTexture(uint32_t velocityTex) {
    m_velocityTexture = velocityTex;
}

void MotionBlurEffect::SetViewProjection(const glm::mat4& current, const glm::mat4& previous) {
    m_currentVP = current;
    m_previousVP = previous;
}

// ============================================================================
// DepthOfFieldEffect Implementation
// ============================================================================

bool DepthOfFieldEffect::Initialize() {
    // DoF shader implementation
    return true;
}

void DepthOfFieldEffect::Shutdown() {
    m_cocShader.reset();
    m_blurShader.reset();
    m_compositeShader.reset();
    if (m_cocFBO) glDeleteFramebuffers(1, &m_cocFBO);
    if (m_cocTexture) glDeleteTextures(1, &m_cocTexture);
    if (m_blurFBO) glDeleteFramebuffers(1, &m_blurFBO);
    if (m_blurTexture) glDeleteTextures(1, &m_blurTexture);
}

void DepthOfFieldEffect::Apply(uint32_t /*inputTexture*/, uint32_t /*outputFBO*/, uint32_t /*depthTexture*/) {
    // DoF implementation
}

void DepthOfFieldEffect::Resize(int width, int height) {
    PostProcessEffect::Resize(width, height);
}

void DepthOfFieldEffect::SetCameraPlanes(float near, float far) {
    m_nearPlane = near;
    m_farPlane = far;
}

void DepthOfFieldEffect::CalculateCoC(uint32_t /*depthTexture*/) {
    // CoC calculation
}

// ============================================================================
// PostProcessPipeline Implementation
// ============================================================================

PostProcessPipeline::~PostProcessPipeline() {
    Shutdown();
}

bool PostProcessPipeline::Initialize(int width, int height, bool hdr) {
    m_width = width;
    m_height = height;
    m_hdr = hdr;

    CreateFramebuffers();

    // Create fullscreen quad VAO
    glGenVertexArrays(1, &m_quadVAO);

    // Create copy shader
    m_copyShader = std::make_unique<Shader>();
    if (!m_copyShader->LoadFromSource(FULLSCREEN_VERTEX_SHADER, COPY_FRAGMENT_SHADER)) {
        LOG_ERROR("Failed to create copy shader");
        return false;
    }

    return true;
}

void PostProcessPipeline::Shutdown() {
    for (auto& effect : m_effects) {
        effect->Shutdown();
    }
    m_effects.clear();
    m_effectsByName.clear();

    if (m_sceneFBO) glDeleteFramebuffers(1, &m_sceneFBO);
    if (m_sceneTexture) glDeleteTextures(1, &m_sceneTexture);
    if (m_sceneDepthRBO) glDeleteRenderbuffers(1, &m_sceneDepthRBO);
    if (m_pingFBO) glDeleteFramebuffers(1, &m_pingFBO);
    if (m_pingTexture) glDeleteTextures(1, &m_pingTexture);
    if (m_pongFBO) glDeleteFramebuffers(1, &m_pongFBO);
    if (m_pongTexture) glDeleteTextures(1, &m_pongTexture);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);

    m_copyShader.reset();
}

void PostProcessPipeline::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    CreateFramebuffers();

    for (auto& effect : m_effects) {
        effect->Resize(width, height);
    }
}

void PostProcessPipeline::CreateFramebuffers() {
    GLenum format = m_hdr ? GL_RGBA16F : GL_RGBA8;

    // Scene framebuffer
    if (m_sceneFBO) glDeleteFramebuffers(1, &m_sceneFBO);
    if (m_sceneTexture) glDeleteTextures(1, &m_sceneTexture);
    if (m_sceneDepthRBO) glDeleteRenderbuffers(1, &m_sceneDepthRBO);

    glGenTextures(1, &m_sceneTexture);
    glBindTexture(GL_TEXTURE_2D, m_sceneTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &m_sceneDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_sceneDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);

    glGenFramebuffers(1, &m_sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_sceneDepthRBO);

    // Ping buffer
    if (m_pingFBO) glDeleteFramebuffers(1, &m_pingFBO);
    if (m_pingTexture) glDeleteTextures(1, &m_pingTexture);

    glGenTextures(1, &m_pingTexture);
    glBindTexture(GL_TEXTURE_2D, m_pingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_pingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pingFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingTexture, 0);

    // Pong buffer
    if (m_pongFBO) glDeleteFramebuffers(1, &m_pongFBO);
    if (m_pongTexture) glDeleteTextures(1, &m_pongTexture);

    glGenTextures(1, &m_pongTexture);
    glBindTexture(GL_TEXTURE_2D, m_pongTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_pongFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pongFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pongTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

PostProcessEffect* PostProcessPipeline::GetEffectByType(PostProcessEffectType type) {
    for (auto& effect : m_effects) {
        if (effect->GetType() == type) {
            return effect.get();
        }
    }
    return nullptr;
}

void PostProcessPipeline::RemoveEffect(const std::string& name) {
    auto it = m_effectsByName.find(name);
    if (it != m_effectsByName.end()) {
        auto effectIt = std::find_if(m_effects.begin(), m_effects.end(),
            [&](const auto& e) { return e.get() == it->second; });
        if (effectIt != m_effects.end()) {
            (*effectIt)->Shutdown();
            m_effects.erase(effectIt);
        }
        m_effectsByName.erase(it);
    }
}

void PostProcessPipeline::SetEffectEnabled(const std::string& name, bool enabled) {
    auto it = m_effectsByName.find(name);
    if (it != m_effectsByName.end()) {
        it->second->SetEnabled(enabled);
    }
}

void PostProcessPipeline::Begin() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessPipeline::End(uint32_t depthTexture) {
    Apply(m_sceneTexture, 0, depthTexture);
}

void PostProcessPipeline::Apply(uint32_t inputTexture, uint32_t outputFBO, uint32_t depthTexture) {
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_quadVAO);

    uint32_t currentInput = inputTexture;
    m_usePing = true;

    // Count enabled effects
    int enabledCount = 0;
    for (const auto& effect : m_effects) {
        if (effect->IsEnabled()) ++enabledCount;
    }

    int processedCount = 0;
    for (auto& effect : m_effects) {
        if (!effect->IsEnabled()) continue;

        ++processedCount;

        // Determine output: final effect goes to outputFBO, others ping-pong
        uint32_t targetFBO = (processedCount == enabledCount) ? outputFBO :
                            (m_usePing ? m_pingFBO : m_pongFBO);
        uint32_t targetTex = m_usePing ? m_pingTexture : m_pongTexture;

        effect->Apply(currentInput, targetFBO, depthTexture);

        if (processedCount < enabledCount) {
            currentInput = targetTex;
            m_usePing = !m_usePing;
        }
    }

    // If no effects, just copy to output
    if (enabledCount == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
        glViewport(0, 0, m_width, m_height);
        m_copyShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTexture);
        m_copyShader->SetInt("u_Texture", 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void PostProcessPipeline::SetViewProjection(const glm::mat4& view, const glm::mat4& projection) {
    m_previousVP = m_projection * m_view;
    m_view = view;
    m_projection = projection;

    // Update SSAO
    if (auto* ssao = dynamic_cast<SSAOEffect*>(GetEffectByType(PostProcessEffectType::AmbientOcclusion))) {
        ssao->SetMatrices(view, projection);
    }

    // Update Motion Blur
    if (auto* mb = dynamic_cast<MotionBlurEffect*>(GetEffectByType(PostProcessEffectType::MotionBlur))) {
        mb->SetViewProjection(projection * view, m_previousVP);
    }
}

void PostProcessPipeline::SetCameraPlanes(float near, float far) {
    m_nearPlane = near;
    m_farPlane = far;

    if (auto* dof = dynamic_cast<DepthOfFieldEffect*>(GetEffectByType(PostProcessEffectType::DepthOfField))) {
        dof->SetCameraPlanes(near, far);
    }
}

std::vector<std::string> PostProcessPipeline::GetEffectNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_effectsByName) {
        names.push_back(name);
    }
    return names;
}

} // namespace Nova
