#include "SVGF.hpp"
#include "Shader.hpp"
#include "../core/Camera.hpp"
#include <glad/glad.h>
#include <iostream>

namespace Nova {

SVGF::SVGF() = default;

SVGF::~SVGF() {
    Shutdown();
}

bool SVGF::Initialize(int width, int height) {
    if (m_initialized) {
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_frameCount = 0;
    m_currentBuffer = 0;

    std::cout << "[SVGF] Initializing " << width << "x" << height << std::endl;

    if (!InitializeBuffers()) {
        std::cerr << "[SVGF] Failed to initialize buffers" << std::endl;
        return false;
    }

    if (!InitializeShaders()) {
        std::cerr << "[SVGF] Failed to initialize shaders" << std::endl;
        CleanupBuffers();
        return false;
    }

    // Create GPU timer queries if profiling is enabled
    if (m_profilingEnabled) {
        glGenQueries(8, m_queryObjects);
    }

    m_initialized = true;
    std::cout << "[SVGF] Initialization successful" << std::endl;
    return true;
}

void SVGF::Shutdown() {
    if (!m_initialized) return;

    CleanupBuffers();

    if (m_profilingEnabled && m_queryObjects[0] != 0) {
        glDeleteQueries(8, m_queryObjects);
        for (int i = 0; i < 8; ++i) {
            m_queryObjects[i] = 0;
        }
    }

    m_temporalAccumulationShader.reset();
    m_varianceEstimationShader.reset();
    m_waveletFilterShader.reset();
    m_finalModulationShader.reset();

    m_initialized = false;
}

void SVGF::Resize(int width, int height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;

    CleanupBuffers();
    InitializeBuffers();
    ResetTemporalHistory();
}

bool SVGF::InitializeBuffers() {
    // Create texture for temporal accumulation (double buffered)
    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &m_accumulatedColor[i]);
        glBindTexture(GL_TEXTURE_2D, m_accumulatedColor[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &m_accumulatedMoments[i]);
        glBindTexture(GL_TEXTURE_2D, m_accumulatedMoments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, m_width, m_height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // History length texture (R16F)
    glGenTextures(1, &m_historyLength);
    glBindTexture(GL_TEXTURE_2D, m_historyLength);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Integrated color (after temporal accumulation)
    glGenTextures(1, &m_integratedColor);
    glBindTexture(GL_TEXTURE_2D, m_integratedColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Variance texture
    glGenTextures(1, &m_variance);
    glBindTexture(GL_TEXTURE_2D, m_variance);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Ping-pong buffers for wavelet filtering
    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &m_pingPongBuffer[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingPongBuffer[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[SVGF] OpenGL error creating buffers: " << error << std::endl;
        return false;
    }

    std::cout << "[SVGF] Created denoising buffers" << std::endl;
    return true;
}

bool SVGF::InitializeShaders() {
    m_temporalAccumulationShader = std::make_unique<Shader>();
    if (!m_temporalAccumulationShader->LoadCompute("assets/shaders/svgf_temporal.comp")) {
        std::cerr << "[SVGF] Failed to load temporal accumulation shader" << std::endl;
        return false;
    }

    m_varianceEstimationShader = std::make_unique<Shader>();
    if (!m_varianceEstimationShader->LoadCompute("assets/shaders/svgf_variance.comp")) {
        std::cerr << "[SVGF] Failed to load variance estimation shader" << std::endl;
        return false;
    }

    m_waveletFilterShader = std::make_unique<Shader>();
    if (!m_waveletFilterShader->LoadCompute("assets/shaders/svgf_wavelet.comp")) {
        std::cerr << "[SVGF] Failed to load wavelet filter shader" << std::endl;
        return false;
    }

    m_finalModulationShader = std::make_unique<Shader>();
    if (!m_finalModulationShader->LoadCompute("assets/shaders/svgf_modulate.comp")) {
        std::cerr << "[SVGF] Failed to load final modulation shader" << std::endl;
        return false;
    }

    return true;
}

void SVGF::CleanupBuffers() {
    for (int i = 0; i < 2; ++i) {
        if (m_accumulatedColor[i] != 0) glDeleteTextures(1, &m_accumulatedColor[i]);
        if (m_accumulatedMoments[i] != 0) glDeleteTextures(1, &m_accumulatedMoments[i]);
        if (m_pingPongBuffer[i] != 0) glDeleteTextures(1, &m_pingPongBuffer[i]);
        m_accumulatedColor[i] = 0;
        m_accumulatedMoments[i] = 0;
        m_pingPongBuffer[i] = 0;
    }

    if (m_historyLength != 0) glDeleteTextures(1, &m_historyLength);
    if (m_integratedColor != 0) glDeleteTextures(1, &m_integratedColor);
    if (m_variance != 0) glDeleteTextures(1, &m_variance);

    m_historyLength = 0;
    m_integratedColor = 0;
    m_variance = 0;
}

void SVGF::Denoise(uint32_t noisyColor,
                  uint32_t gBufferPosition,
                  uint32_t gBufferNormal,
                  uint32_t gBufferAlbedo,
                  uint32_t gBufferDepth,
                  uint32_t motionVectors,
                  uint32_t outputTexture) {
    if (!m_initialized) return;

    m_stats = Stats();

    // 1. Temporal accumulation
    if (m_settings.temporalAccumulation) {
        BeginProfile("TemporalAccumulation");
        TemporalAccumulation(noisyColor, gBufferPosition, gBufferNormal, gBufferDepth, motionVectors);
        EndProfile(m_stats.temporalAccumulationMs);
    } else {
        // Copy noisy color directly to integrated color
        glCopyImageSubData(noisyColor, GL_TEXTURE_2D, 0, 0, 0, 0,
                          m_integratedColor, GL_TEXTURE_2D, 0, 0, 0, 0,
                          m_width, m_height, 1);
    }

    // 2. Variance estimation
    BeginProfile("VarianceEstimation");
    EstimateVariance(gBufferPosition, gBufferNormal);
    EndProfile(m_stats.varianceEstimationMs);

    // 3. Wavelet filtering (multiple iterations)
    BeginProfile("WaveletFilter");
    for (int i = 0; i < m_settings.waveletIterations; ++i) {
        WaveletFilter(i, gBufferPosition, gBufferNormal, gBufferDepth);
        SwapPingPong();
    }
    EndProfile(m_stats.waveletFilterMs);

    // 4. Final modulation
    BeginProfile("FinalModulation");
    FinalModulation(gBufferAlbedo, outputTexture);
    EndProfile(m_stats.finalModulationMs);

    m_stats.totalMs = m_stats.temporalAccumulationMs + m_stats.varianceEstimationMs +
                      m_stats.waveletFilterMs + m_stats.finalModulationMs;

    m_frameCount++;
}

void SVGF::TemporalAccumulation(uint32_t noisyColor,
                               uint32_t gBufferPosition,
                               uint32_t gBufferNormal,
                               uint32_t gBufferDepth,
                               uint32_t motionVectors) {
    if (!m_temporalAccumulationShader) return;

    m_temporalAccumulationShader->Use();

    // Bind input textures
    glBindImageTexture(0, noisyColor, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(3, gBufferDepth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(4, motionVectors, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);

    // Bind history (read from previous frame)
    int readBuffer = 1 - m_currentBuffer;
    glBindImageTexture(5, m_accumulatedColor[readBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(6, m_accumulatedMoments[readBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(7, m_historyLength, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16F);

    // Bind output (write to current frame)
    int writeBuffer = m_currentBuffer;
    glBindImageTexture(8, m_accumulatedColor[writeBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(9, m_accumulatedMoments[writeBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
    glBindImageTexture(10, m_integratedColor, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Set uniforms
    m_temporalAccumulationShader->SetInt("u_frameCount", m_frameCount);
    m_temporalAccumulationShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_temporalAccumulationShader->SetFloat("u_alpha", m_settings.temporalAlpha);
    m_temporalAccumulationShader->SetFloat("u_maxM", m_settings.temporalMaxM);
    m_temporalAccumulationShader->SetFloat("u_depthThreshold", m_settings.temporalDepthThreshold);
    m_temporalAccumulationShader->SetFloat("u_normalThreshold", m_settings.temporalNormalThreshold);

    // Dispatch
    const int groupSizeX = 8;
    const int groupSizeY = 8;
    int numGroupsX = (m_width + groupSizeX - 1) / groupSizeX;
    int numGroupsY = (m_height + groupSizeY - 1) / groupSizeY;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    m_currentBuffer = writeBuffer;
}

void SVGF::EstimateVariance(uint32_t gBufferPosition, uint32_t gBufferNormal) {
    if (!m_varianceEstimationShader) return;

    m_varianceEstimationShader->Use();

    // Bind inputs
    glBindImageTexture(0, m_integratedColor, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, m_accumulatedMoments[m_currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(2, m_historyLength, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(3, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(4, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);

    // Bind output
    glBindImageTexture(5, m_variance, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);

    // Set uniforms
    m_varianceEstimationShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_varianceEstimationShader->SetInt("u_kernelSize", m_settings.varianceKernelSize);
    m_varianceEstimationShader->SetFloat("u_varianceBoost", m_settings.varianceBoost);

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void SVGF::WaveletFilter(int iteration,
                        uint32_t gBufferPosition,
                        uint32_t gBufferNormal,
                        uint32_t gBufferDepth) {
    if (!m_waveletFilterShader) return;

    m_waveletFilterShader->Use();

    // For first iteration, read from integrated color, otherwise from ping-pong buffer
    uint32_t inputTexture = (iteration == 0) ? m_integratedColor : m_pingPongBuffer[1 - (iteration % 2)];
    uint32_t outputTexture = m_pingPongBuffer[iteration % 2];

    // Bind textures
    glBindImageTexture(0, inputTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, m_variance, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(2, m_historyLength, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(3, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(4, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(5, gBufferDepth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(6, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Set uniforms
    m_waveletFilterShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_waveletFilterShader->SetInt("u_iteration", iteration);
    m_waveletFilterShader->SetFloat("u_phiColor", m_settings.phiColor);
    m_waveletFilterShader->SetFloat("u_phiNormal", m_settings.phiNormal);
    m_waveletFilterShader->SetFloat("u_phiDepth", m_settings.phiDepth);
    m_waveletFilterShader->SetFloat("u_sigmaLuminance", m_settings.sigmaLuminance);
    m_waveletFilterShader->SetBool("u_useVarianceGuidance", m_settings.useVarianceGuidance);
    m_waveletFilterShader->SetBool("u_adaptiveKernel", m_settings.adaptiveKernel);

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void SVGF::FinalModulation(uint32_t gBufferAlbedo, uint32_t outputTexture) {
    if (!m_finalModulationShader) return;

    m_finalModulationShader->Use();

    // Get final filtered result
    int finalIteration = (m_settings.waveletIterations - 1) % 2;
    uint32_t filteredColor = m_pingPongBuffer[finalIteration];

    // Bind textures
    glBindImageTexture(0, filteredColor, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, gBufferAlbedo, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(2, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Set uniforms
    m_finalModulationShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void SVGF::ResetTemporalHistory() {
    if (!m_initialized) return;

    // Clear history length to 0
    std::vector<float> zeros(m_width * m_height, 0.0f);

    glBindTexture(GL_TEXTURE_2D, m_historyLength);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_FLOAT, zeros.data());

    glBindTexture(GL_TEXTURE_2D, 0);
    m_frameCount = 0;
}

void SVGF::BeginProfile(const char* label) {
    if (!m_profilingEnabled || m_queryObjects[0] == 0) return;

    static int queryIndex = 0;
    glBeginQuery(GL_TIME_ELAPSED, m_queryObjects[queryIndex]);
    queryIndex = (queryIndex + 1) % 8;
}

void SVGF::EndProfile(float& outTimeMs) {
    if (!m_profilingEnabled || m_queryObjects[0] == 0) {
        outTimeMs = 0.0f;
        return;
    }

    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 elapsedTime = 0;
    glGetQueryObjectui64v(m_queryObjects[0], GL_QUERY_RESULT, &elapsedTime);
    outTimeMs = elapsedTime / 1000000.0f;
}

void SVGF::SwapPingPong() {
    // Nothing to do - we handle ping-pong in WaveletFilter
}

} // namespace Nova
