#include "SDFBatchRenderer.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>

namespace Engine {
namespace Graphics {

SDFBatchRenderer::SDFBatchRenderer(const Config& config)
    : m_config(config) {
    m_instances.reserve(10000);
    m_batches.reserve(config.maxBatches);
}

SDFBatchRenderer::~SDFBatchRenderer() {
}

bool SDFBatchRenderer::Initialize() {
    // Create batched instance buffer
    m_batchedInstanceBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_batchedInstanceBuffer->Allocate(m_config.maxInstancesPerBatch * sizeof(GPUInstanceData));

    // Create multi-draw renderer
    m_multiDrawRenderer = std::make_unique<MultiDrawIndirectRenderer>();

    return true;
}

void SDFBatchRenderer::AddInstance(const SDFInstance& instance) {
    m_instances.push_back(instance);

    // Group by material
    m_materialGroups[instance.materialID].push_back(static_cast<uint32_t>(m_instances.size() - 1));

    m_stats.totalInstances++;
}

void SDFBatchRenderer::BuildBatches() {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_batches.clear();

    // Create batch for each material
    for (const auto& pair : m_materialGroups) {
        uint32_t materialID = pair.first;
        const auto& indices = pair.second;

        if (indices.empty()) continue;

        SDFMaterialBatch batch;
        batch.materialID = materialID;
        batch.instanceIndices = indices;

        // Setup draw command
        batch.drawCommand.vertexCount = 36;  // Cube has 36 vertices (12 triangles)
        batch.drawCommand.instanceCount = static_cast<uint32_t>(indices.size());
        batch.drawCommand.firstVertex = 0;
        batch.drawCommand.baseVertex = 0;
        batch.drawCommand.baseInstance = static_cast<uint32_t>(m_batches.size());

        m_batches.push_back(batch);
    }

    // Sort batches if enabled
    if (m_config.enableStateSorting) {
        SortBatches();
    }

    m_stats.totalBatches = static_cast<uint32_t>(m_batches.size());

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.batchingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void SDFBatchRenderer::SortBatches() {
    // Sort by shader program, then material ID to minimize state changes
    std::sort(m_batches.begin(), m_batches.end(),
        [](const SDFMaterialBatch& a, const SDFMaterialBatch& b) {
            if (a.shaderProgram != b.shaderProgram) {
                return a.shaderProgram < b.shaderProgram;
            }
            return a.materialID < b.materialID;
        }
    );
}

void SDFBatchRenderer::RenderBatches(const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_stats.drawCallCount = 0;
    m_stats.stateChanges = 0;

    if (m_config.enableInstancing && m_multiDrawRenderer) {
        // Use multi-draw indirect for maximum performance
        m_multiDrawRenderer->Clear();

        for (const auto& batch : m_batches) {
            m_multiDrawRenderer->AddDrawCommand(batch.drawCommand);
        }

        // Single multi-draw call for all batches
        m_multiDrawRenderer->ExecuteMultiDraw();
        m_stats.drawCallCount = 1;

    } else {
        // Render each batch separately
        for (size_t i = 0; i < m_batches.size(); i++) {
            RenderBatch(static_cast<uint32_t>(i), viewMatrix, projMatrix);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void SDFBatchRenderer::RenderBatch(
    uint32_t batchIndex,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix) {

    if (batchIndex >= m_batches.size()) return;

    const SDFMaterialBatch& batch = m_batches[batchIndex];

    // Setup shader state
    ShaderState state;
    state.program = batch.shaderProgram;
    state.blendMode = 0;  // Opaque
    state.depthMode = 1;  // Depth test enabled
    state.alphaTesting = false;

    if (!(state == m_currentState)) {
        ApplyShaderState(state);
        m_currentState = state;
        m_stats.stateChanges++;
    }

    // Bind shader
    glUseProgram(batch.shaderProgram);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(batch.shaderProgram, "u_view");
    GLint projLoc = glGetUniformLocation(batch.shaderProgram, "u_proj");

    if (viewLoc >= 0) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix.m);
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix.m);

    // Draw instanced
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, batch.drawCommand.instanceCount);

    m_stats.drawCallCount++;
}

void SDFBatchRenderer::ApplyShaderState(const ShaderState& state) {
    // Apply blend mode
    if (state.blendMode == 0) {
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Apply depth mode
    if (state.depthMode == 0) {
        glDisable(GL_DEPTH_TEST);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    // Alpha testing
    if (state.alphaTesting) {
        // Enable alpha test (or discard in shader)
    }
}

void SDFBatchRenderer::Clear() {
    m_instances.clear();
    m_batches.clear();
    m_materialGroups.clear();
    m_stats.totalInstances = 0;
    m_stats.totalBatches = 0;
}

void SDFBatchRenderer::ResetStats() {
    m_stats = Stats();
}

} // namespace Graphics
} // namespace Engine
