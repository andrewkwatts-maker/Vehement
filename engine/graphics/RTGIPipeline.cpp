#include "RTGIPipeline.hpp"
#include "ReSTIR.hpp"
#include "SVGF.hpp"
#include "ClusteredLighting.hpp"
#include "../core/Camera.hpp"
#include <glad/gl.h>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace Nova {

RTGIPipeline::RTGIPipeline() = default;

RTGIPipeline::~RTGIPipeline() {
    Shutdown();
}

bool RTGIPipeline::Initialize(int width, int height) {
    if (m_initialized) {
        Shutdown();
    }

    m_width = width;
    m_height = height;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Real-Time Global Illumination Pipeline" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Resolution: " << width << "x" << height << std::endl;
    std::cout << "Target: 120 FPS (8.3ms per frame)" << std::endl;
    std::cout << "----------------------------------------\n" << std::endl;

    // Initialize ReSTIR
    m_restir = std::make_unique<ReSTIRGI>();
    if (!m_restir->Initialize(width, height)) {
        std::cerr << "[RTGI] Failed to initialize ReSTIR" << std::endl;
        return false;
    }
    m_restir->SetProfilingEnabled(m_profilingEnabled);

    // Initialize SVGF
    m_svgf = std::make_unique<SVGF>();
    if (!m_svgf->Initialize(width, height)) {
        std::cerr << "[RTGI] Failed to initialize SVGF" << std::endl;
        return false;
    }
    m_svgf->SetProfilingEnabled(m_profilingEnabled);

    // Initialize intermediate buffers
    if (!InitializeBuffers()) {
        std::cerr << "[RTGI] Failed to initialize buffers" << std::endl;
        return false;
    }

    // Apply default quality preset (Medium = 120 FPS target)
    ApplyQualityPreset(QualityPreset::Medium);

    m_initialized = true;
    std::cout << "[RTGI] Pipeline initialization successful!\n" << std::endl;

    return true;
}

void RTGIPipeline::Shutdown() {
    if (!m_initialized) return;

    CleanupBuffers();

    if (m_restir) m_restir->Shutdown();
    if (m_svgf) m_svgf->Shutdown();

    m_restir.reset();
    m_svgf.reset();

    m_initialized = false;
}

void RTGIPipeline::Resize(int width, int height) {
    if (m_width == width && m_height == height) return;

    std::cout << "[RTGI] Resizing to " << width << "x" << height << std::endl;

    m_width = width;
    m_height = height;

    if (m_restir) m_restir->Resize(width, height);
    if (m_svgf) m_svgf->Resize(width, height);

    CleanupBuffers();
    InitializeBuffers();
}

bool RTGIPipeline::InitializeBuffers() {
    // Create intermediate buffer for ReSTIR output
    glGenTextures(1, &m_restirOutput);
    glBindTexture(GL_TEXTURE_2D, m_restirOutput);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[RTGI] OpenGL error creating buffers: " << error << std::endl;
        return false;
    }

    return true;
}

void RTGIPipeline::CleanupBuffers() {
    if (m_restirOutput != 0) {
        glDeleteTextures(1, &m_restirOutput);
        m_restirOutput = 0;
    }
}

void RTGIPipeline::Render(const Camera& camera,
                         ClusteredLightManager& lightManager,
                         uint32_t gBufferPosition,
                         uint32_t gBufferNormal,
                         uint32_t gBufferAlbedo,
                         uint32_t gBufferDepth,
                         uint32_t motionVectors,
                         uint32_t outputTexture) {
    if (!m_initialized) return;

    // Start frame timing using high-resolution clock
    auto frameStartTime = std::chrono::high_resolution_clock::now();

    // 1. ReSTIR Pass - Generate high-quality light samples
    if (m_restirEnabled && m_restir) {
        m_restir->Execute(camera, lightManager,
                         gBufferPosition, gBufferNormal, gBufferAlbedo,
                         gBufferDepth, motionVectors,
                         m_restirOutput);
    } else {
        // If ReSTIR disabled, just clear to black
        glClearTexImage(m_restirOutput, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    // 2. SVGF Pass - Denoise ReSTIR output
    if (m_svgfEnabled && m_svgf) {
        m_svgf->Denoise(m_restirOutput,
                       gBufferPosition, gBufferNormal, gBufferAlbedo,
                       gBufferDepth, motionVectors,
                       outputTexture);
    } else {
        // If SVGF disabled, copy ReSTIR output directly
        glCopyImageSubData(m_restirOutput, GL_TEXTURE_2D, 0, 0, 0, 0,
                          outputTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
                          m_width, m_height, 1);
    }

    // Calculate elapsed time and store for statistics
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> elapsed = frameEndTime - frameStartTime;
    m_lastFrameTime = elapsed.count();

    // Update statistics
    UpdateStats();

    m_frameCount++;
}

void RTGIPipeline::ApplyQualityPreset(QualityPreset preset) {
    if (!m_restir || !m_svgf) return;

    auto restirSettings = m_restir->GetSettings();
    auto svgfSettings = m_svgf->GetSettings();

    switch (preset) {
        case QualityPreset::Ultra:
            std::cout << "[RTGI] Applying Ultra quality preset (60 FPS target)" << std::endl;
            // ReSTIR settings
            restirSettings.initialCandidates = 64;
            restirSettings.spatialIterations = 4;
            restirSettings.spatialSamples = 10;
            restirSettings.temporalMaxM = 40.0f;

            // SVGF settings
            svgfSettings.waveletIterations = 5;
            svgfSettings.varianceKernelSize = 5;
            svgfSettings.temporalMaxM = 64.0f;
            break;

        case QualityPreset::High:
            std::cout << "[RTGI] Applying High quality preset (90 FPS target)" << std::endl;
            restirSettings.initialCandidates = 48;
            restirSettings.spatialIterations = 3;
            restirSettings.spatialSamples = 8;
            restirSettings.temporalMaxM = 30.0f;

            svgfSettings.waveletIterations = 5;
            svgfSettings.varianceKernelSize = 3;
            svgfSettings.temporalMaxM = 48.0f;
            break;

        case QualityPreset::Medium:
            std::cout << "[RTGI] Applying Medium quality preset (120 FPS target)" << std::endl;
            restirSettings.initialCandidates = 32;
            restirSettings.spatialIterations = 3;
            restirSettings.spatialSamples = 5;
            restirSettings.temporalMaxM = 20.0f;

            svgfSettings.waveletIterations = 5;
            svgfSettings.varianceKernelSize = 3;
            svgfSettings.temporalMaxM = 32.0f;
            break;

        case QualityPreset::Low:
            std::cout << "[RTGI] Applying Low quality preset (144+ FPS target)" << std::endl;
            restirSettings.initialCandidates = 16;
            restirSettings.spatialIterations = 2;
            restirSettings.spatialSamples = 4;
            restirSettings.temporalMaxM = 16.0f;

            svgfSettings.waveletIterations = 4;
            svgfSettings.varianceKernelSize = 3;
            svgfSettings.temporalMaxM = 24.0f;
            break;

        case QualityPreset::VeryLow:
            std::cout << "[RTGI] Applying Very Low quality preset (240+ FPS target)" << std::endl;
            restirSettings.initialCandidates = 8;
            restirSettings.spatialIterations = 1;
            restirSettings.spatialSamples = 3;
            restirSettings.temporalMaxM = 8.0f;

            svgfSettings.waveletIterations = 3;
            svgfSettings.varianceKernelSize = 3;
            svgfSettings.temporalMaxM = 16.0f;
            break;
    }

    m_restir->SetSettings(restirSettings);
    m_svgf->SetSettings(svgfSettings);
}

void RTGIPipeline::SetProfilingEnabled(bool enabled) {
    m_profilingEnabled = enabled;
    if (m_restir) m_restir->SetProfilingEnabled(enabled);
    if (m_svgf) m_svgf->SetProfilingEnabled(enabled);
}

void RTGIPipeline::ResetTemporalHistory() {
    std::cout << "[RTGI] Resetting temporal history" << std::endl;
    if (m_svgf) m_svgf->ResetTemporalHistory();
    m_frameCount = 0;
}

void RTGIPipeline::UpdateStats() {
    if (!m_restir || !m_svgf) return;

    // Get sub-system stats
    auto restirStats = m_restir->GetStats();
    auto svgfStats = m_svgf->GetStats();

    // Combine timings
    m_stats.restirMs = restirStats.totalMs;
    m_stats.svgfMs = svgfStats.totalMs;
    m_stats.totalMs = m_stats.restirMs + m_stats.svgfMs;

    // Calculate FPS
    if (m_stats.totalMs > 0.0f) {
        m_stats.frameTimeMs = m_stats.totalMs;
        m_stats.currentFPS = 1000.0f / m_stats.totalMs;

        // Update rolling average
        m_fpsHistory[m_fpsHistoryIndex] = m_stats.currentFPS;
        m_fpsHistoryIndex = (m_fpsHistoryIndex + 1) % 60;

        float sum = 0.0f;
        for (int i = 0; i < 60; ++i) sum += m_fpsHistory[i];
        m_stats.avgFPS = sum / 60.0f;
    }

    // Estimate effective SPP
    auto restirSettings = m_restir->GetSettings();
    uint32_t initialSamples = restirSettings.initialCandidates;
    uint32_t temporalM = static_cast<uint32_t>(restirSettings.temporalMaxM);
    uint32_t spatialSamples = restirSettings.spatialSamples * restirSettings.spatialIterations;
    uint32_t svgfPasses = m_svgf->GetSettings().waveletIterations;

    m_stats.effectiveSPP = initialSamples * temporalM * spatialSamples * svgfPasses;

    // Copy other metrics
    m_stats.temporalReuseRate = restirStats.temporalReuseRate;
    m_stats.spatialReuseRate = svgfStats.disocclusionRate;
}

void RTGIPipeline::PrintPerformanceReport() const {
    if (!m_restir || !m_svgf) return;

    auto restirStats = m_restir->GetStats();
    auto svgfStats = m_svgf->GetStats();

    std::cout << "\n========================================" << std::endl;
    std::cout << "RTGI Performance Report" << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << std::fixed << std::setprecision(2);

    // Overall stats
    std::cout << "\nOverall:" << std::endl;
    std::cout << "  Total Time:      " << m_stats.totalMs << " ms" << std::endl;
    std::cout << "  Current FPS:     " << m_stats.currentFPS << std::endl;
    std::cout << "  Average FPS:     " << m_stats.avgFPS << std::endl;
    std::cout << "  Effective SPP:   " << m_stats.effectiveSPP << std::endl;

    // ReSTIR breakdown
    std::cout << "\nReSTIR Breakdown:" << std::endl;
    std::cout << "  Initial Sampling:  " << restirStats.initialSamplingMs << " ms" << std::endl;
    std::cout << "  Temporal Reuse:    " << restirStats.temporalReuseMs << " ms" << std::endl;
    std::cout << "  Spatial Reuse:     " << restirStats.spatialReuseMs << " ms" << std::endl;
    std::cout << "  Final Shading:     " << restirStats.finalShadingMs << " ms" << std::endl;
    std::cout << "  Total ReSTIR:      " << restirStats.totalMs << " ms" << std::endl;

    // SVGF breakdown
    std::cout << "\nSVGF Breakdown:" << std::endl;
    std::cout << "  Temporal Accum:    " << svgfStats.temporalAccumulationMs << " ms" << std::endl;
    std::cout << "  Variance Est:      " << svgfStats.varianceEstimationMs << " ms" << std::endl;
    std::cout << "  Wavelet Filter:    " << svgfStats.waveletFilterMs << " ms" << std::endl;
    std::cout << "  Final Modulation:  " << svgfStats.finalModulationMs << " ms" << std::endl;
    std::cout << "  Total SVGF:        " << svgfStats.totalMs << " ms" << std::endl;

    // Performance targets
    std::cout << "\nPerformance Targets:" << std::endl;
    std::cout << "  120 FPS target:    8.33 ms" << std::endl;
    std::cout << "  90 FPS target:     11.11 ms" << std::endl;
    std::cout << "  60 FPS target:     16.67 ms" << std::endl;

    if (m_stats.totalMs <= 8.33f) {
        std::cout << "  Status: ✓ EXCEEDS 120 FPS target!" << std::endl;
    } else if (m_stats.totalMs <= 11.11f) {
        std::cout << "  Status: ✓ Meets 90 FPS target" << std::endl;
    } else if (m_stats.totalMs <= 16.67f) {
        std::cout << "  Status: ✓ Meets 60 FPS target" << std::endl;
    } else {
        std::cout << "  Status: ✗ Below 60 FPS" << std::endl;
    }

    std::cout << "========================================\n" << std::endl;
}

} // namespace Nova
