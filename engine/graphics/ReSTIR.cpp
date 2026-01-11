#include "ReSTIR.hpp"
#include "Shader.hpp"
#include "ClusteredLighting.hpp"
#include "../core/Camera.hpp"
#include <glad/glad.h>
#include <iostream>

namespace Nova {

ReSTIRGI::ReSTIRGI() = default;

ReSTIRGI::~ReSTIRGI() {
    Shutdown();
}

bool ReSTIRGI::Initialize(int width, int height) {
    if (m_initialized) {
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_frameCount = 0;
    m_currentReservoir = 0;

    std::cout << "[ReSTIR] Initializing " << width << "x" << height << std::endl;

    if (!InitializeBuffers()) {
        std::cerr << "[ReSTIR] Failed to initialize buffers" << std::endl;
        return false;
    }

    if (!InitializeShaders()) {
        std::cerr << "[ReSTIR] Failed to initialize shaders" << std::endl;
        CleanupBuffers();
        return false;
    }

    // Create GPU timer queries if profiling is enabled
    if (m_profilingEnabled) {
        glGenQueries(8, m_queryObjects);
    }

    m_initialized = true;
    std::cout << "[ReSTIR] Initialization successful" << std::endl;
    return true;
}

void ReSTIRGI::Shutdown() {
    if (!m_initialized) return;

    CleanupBuffers();

    if (m_profilingEnabled && m_queryObjects[0] != 0) {
        glDeleteQueries(8, m_queryObjects);
        for (int i = 0; i < 8; ++i) {
            m_queryObjects[i] = 0;
        }
    }

    m_initialSamplingShader.reset();
    m_temporalReuseShader.reset();
    m_spatialReuseShader.reset();
    m_finalShadingShader.reset();

    m_initialized = false;
}

void ReSTIRGI::Resize(int width, int height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;

    CleanupBuffers();
    InitializeBuffers();
}

bool ReSTIRGI::InitializeBuffers() {
    const size_t pixelCount = m_width * m_height;
    const size_t reservoirSize = pixelCount * sizeof(Reservoir);

    // Create double-buffered reservoir storage
    glGenBuffers(2, m_reservoirBuffer);

    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_reservoirBuffer[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, reservoirSize, nullptr, GL_DYNAMIC_COPY);

        // Initialize with empty reservoirs
        std::vector<Reservoir> emptyReservoirs(pixelCount);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, reservoirSize, emptyReservoirs.data());
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[ReSTIR] OpenGL error creating buffers: " << error << std::endl;
        return false;
    }

    std::cout << "[ReSTIR] Created reservoir buffers: " << (reservoirSize / 1024.0f / 1024.0f)
              << " MB each" << std::endl;

    return true;
}

bool ReSTIRGI::InitializeShaders() {
    // Load compute shaders
    m_initialSamplingShader = std::make_unique<Shader>();
    if (!m_initialSamplingShader->LoadCompute("assets/shaders/restir_initial.comp")) {
        std::cerr << "[ReSTIR] Failed to load initial sampling shader" << std::endl;
        return false;
    }

    m_temporalReuseShader = std::make_unique<Shader>();
    if (!m_temporalReuseShader->LoadCompute("assets/shaders/restir_temporal.comp")) {
        std::cerr << "[ReSTIR] Failed to load temporal reuse shader" << std::endl;
        return false;
    }

    m_spatialReuseShader = std::make_unique<Shader>();
    if (!m_spatialReuseShader->LoadCompute("assets/shaders/restir_spatial.comp")) {
        std::cerr << "[ReSTIR] Failed to load spatial reuse shader" << std::endl;
        return false;
    }

    m_finalShadingShader = std::make_unique<Shader>();
    if (!m_finalShadingShader->LoadCompute("assets/shaders/restir_final.comp")) {
        std::cerr << "[ReSTIR] Failed to load final shading shader" << std::endl;
        return false;
    }

    return true;
}

void ReSTIRGI::CleanupBuffers() {
    if (m_reservoirBuffer[0] != 0) {
        glDeleteBuffers(2, m_reservoirBuffer);
        m_reservoirBuffer[0] = 0;
        m_reservoirBuffer[1] = 0;
    }
}

void ReSTIRGI::Execute(const Camera& camera,
                      ClusteredLightManager& lightManager,
                      uint32_t gBufferPosition,
                      uint32_t gBufferNormal,
                      uint32_t gBufferAlbedo,
                      uint32_t gBufferDepth,
                      uint32_t motionVectors,
                      uint32_t outputTexture) {
    if (!m_initialized) return;

    // Reset stats
    m_stats = Stats();
    float startTime = 0.0f;

    // 1. Generate initial light samples
    BeginProfile("InitialSampling");
    GenerateInitialSamples(camera, lightManager, gBufferPosition, gBufferNormal, gBufferAlbedo);
    EndProfile(m_stats.initialSamplingMs);

    // 2. Temporal reuse (optional)
    if (m_settings.temporalReuse && m_frameCount > 0) {
        BeginProfile("TemporalReuse");
        TemporalReuse(gBufferPosition, gBufferNormal, motionVectors);
        EndProfile(m_stats.temporalReuseMs);
    }

    // 3. Spatial reuse (multiple iterations)
    BeginProfile("SpatialReuse");
    for (int i = 0; i < m_settings.spatialIterations; ++i) {
        SpatialReuse(gBufferPosition, gBufferNormal, gBufferDepth);
        SwapReservoirBuffers();  // Ping-pong between buffers
    }
    EndProfile(m_stats.spatialReuseMs);

    // 4. Final shading
    BeginProfile("FinalShading");
    FinalShading(lightManager, gBufferPosition, gBufferNormal, gBufferAlbedo, outputTexture);
    EndProfile(m_stats.finalShadingMs);

    m_stats.totalMs = m_stats.initialSamplingMs + m_stats.temporalReuseMs +
                      m_stats.spatialReuseMs + m_stats.finalShadingMs;

    // Increment frame counter
    m_frameCount++;
}

void ReSTIRGI::GenerateInitialSamples(const Camera& camera,
                                     ClusteredLightManager& lightManager,
                                     uint32_t gBufferPosition,
                                     uint32_t gBufferNormal,
                                     uint32_t gBufferAlbedo) {
    if (!m_initialSamplingShader) return;

    m_initialSamplingShader->Use();

    // Bind G-buffers
    glBindImageTexture(0, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(2, gBufferAlbedo, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

    // Bind output reservoir buffer (write to current)
    int writeBuffer = m_currentReservoir;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_reservoirBuffer[writeBuffer]);

    // Bind light buffer from clustered lighting
    lightManager.BindForRendering(1, 2, 3);

    // Set uniforms
    m_initialSamplingShader->SetInt("u_initialCandidates", m_settings.initialCandidates);
    m_initialSamplingShader->SetInt("u_frameCount", m_frameCount);
    m_initialSamplingShader->SetInt("u_lightCount", lightManager.GetLightCount());
    m_initialSamplingShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_initialSamplingShader->SetMat4("u_viewMatrix", camera.GetViewMatrix());
    m_initialSamplingShader->SetMat4("u_projMatrix", camera.GetProjectionMatrix());

    // Dispatch compute shader
    const int groupSizeX = 8;
    const int groupSizeY = 8;
    int numGroupsX = (m_width + groupSizeX - 1) / groupSizeX;
    int numGroupsY = (m_height + groupSizeY - 1) / groupSizeY;

    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ReSTIRGI::TemporalReuse(uint32_t gBufferPosition,
                            uint32_t gBufferNormal,
                            uint32_t motionVectors) {
    if (!m_temporalReuseShader) return;

    m_temporalReuseShader->Use();

    // Bind G-buffers
    glBindImageTexture(0, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(2, motionVectors, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);

    // Bind reservoir buffers (read from previous frame, write to current)
    int readBuffer = 1 - m_currentReservoir;   // Previous frame
    int writeBuffer = m_currentReservoir;      // Current frame

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_reservoirBuffer[readBuffer]);  // Previous
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_reservoirBuffer[writeBuffer]); // Current

    // Set uniforms
    m_temporalReuseShader->SetInt("u_frameCount", m_frameCount);
    m_temporalReuseShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_temporalReuseShader->SetFloat("u_maxM", m_settings.temporalMaxM);
    m_temporalReuseShader->SetFloat("u_depthThreshold", m_settings.temporalDepthThreshold);
    m_temporalReuseShader->SetFloat("u_normalThreshold", m_settings.temporalNormalThreshold);

    // Dispatch
    const int groupSizeX = 8;
    const int groupSizeY = 8;
    int numGroupsX = (m_width + groupSizeX - 1) / groupSizeX;
    int numGroupsY = (m_height + groupSizeY - 1) / groupSizeY;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ReSTIRGI::SpatialReuse(uint32_t gBufferPosition,
                           uint32_t gBufferNormal,
                           uint32_t gBufferDepth) {
    if (!m_spatialReuseShader) return;

    m_spatialReuseShader->Use();

    // Bind G-buffers
    glBindImageTexture(0, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(2, gBufferDepth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

    // Bind reservoir buffers (read and write)
    int readBuffer = m_currentReservoir;
    int writeBuffer = 1 - m_currentReservoir;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_reservoirBuffer[readBuffer]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_reservoirBuffer[writeBuffer]);

    // Set uniforms
    m_spatialReuseShader->SetInt("u_frameCount", m_frameCount);
    m_spatialReuseShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_spatialReuseShader->SetFloat("u_spatialRadius", m_settings.spatialRadius);
    m_spatialReuseShader->SetInt("u_spatialSamples", m_settings.spatialSamples);
    m_spatialReuseShader->SetBool("u_discardHistory", m_settings.spatialDiscardHistory);

    // Dispatch
    const int groupSizeX = 8;
    const int groupSizeY = 8;
    int numGroupsX = (m_width + groupSizeX - 1) / groupSizeX;
    int numGroupsY = (m_height + groupSizeY - 1) / groupSizeY;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ReSTIRGI::FinalShading(ClusteredLightManager& lightManager,
                           uint32_t gBufferPosition,
                           uint32_t gBufferNormal,
                           uint32_t gBufferAlbedo,
                           uint32_t outputTexture) {
    if (!m_finalShadingShader) return;

    m_finalShadingShader->Use();

    // Bind G-buffers
    glBindImageTexture(0, gBufferPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, gBufferNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB16F);
    glBindImageTexture(2, gBufferAlbedo, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(3, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Bind final reservoir buffer
    int readBuffer = m_currentReservoir;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_reservoirBuffer[readBuffer]);

    // Bind light buffer
    lightManager.BindForRendering(1, 2, 3);

    // Set uniforms
    m_finalShadingShader->SetInt("u_lightCount", lightManager.GetLightCount());
    m_finalShadingShader->SetVec2("u_resolution", glm::vec2(m_width, m_height));
    m_finalShadingShader->SetBool("u_biasCorrection", m_settings.biasCorrection);
    m_finalShadingShader->SetFloat("u_rayOffset", m_settings.biasRayOffset);

    // Dispatch
    const int groupSizeX = 8;
    const int groupSizeY = 8;
    int numGroupsX = (m_width + groupSizeX - 1) / groupSizeX;
    int numGroupsY = (m_height + groupSizeY - 1) / groupSizeY;

    glDispatchCompute(numGroupsX, numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void ReSTIRGI::BeginProfile(const char* label) {
    if (!m_profilingEnabled || m_queryObjects[0] == 0) return;

    // Use timer query for GPU timing
    static int queryIndex = 0;
    glBeginQuery(GL_TIME_ELAPSED, m_queryObjects[queryIndex]);
    queryIndex = (queryIndex + 1) % 8;
}

void ReSTIRGI::EndProfile(float& outTimeMs) {
    if (!m_profilingEnabled || m_queryObjects[0] == 0) {
        outTimeMs = 0.0f;
        return;
    }

    glEndQuery(GL_TIME_ELAPSED);

    // Get previous query result (to avoid stalling the pipeline)
    GLuint64 elapsedTime = 0;
    glGetQueryObjectui64v(m_queryObjects[0], GL_QUERY_RESULT, &elapsedTime);
    outTimeMs = elapsedTime / 1000000.0f; // Convert nanoseconds to milliseconds
}

void ReSTIRGI::SwapReservoirBuffers() {
    m_currentReservoir = 1 - m_currentReservoir;
}

} // namespace Nova
