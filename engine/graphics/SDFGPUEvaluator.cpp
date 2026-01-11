#include "SDFGPUEvaluator.hpp"
#include <glad/glad.h>
#include <chrono>
#include <cstring>

namespace Nova {

SDFGPUEvaluator::SDFGPUEvaluator() {}

SDFGPUEvaluator::~SDFGPUEvaluator() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool SDFGPUEvaluator::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Create SSBO for instruction bytecode
    glGenBuffers(1, &m_instructionSSBO);
    if (m_instructionSSBO == 0) {
        return false;
    }

    // Initialize with empty buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instructionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_initialized = true;
    return true;
}

void SDFGPUEvaluator::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_instructionSSBO != 0) {
        glDeleteBuffers(1, &m_instructionSSBO);
        m_instructionSSBO = 0;
    }

    m_instructions.clear();
    m_initialized = false;
}

// =============================================================================
// Bytecode Building
// =============================================================================

void SDFGPUEvaluator::Clear() {
    m_instructions.clear();
    m_stats = Stats();
}

void SDFGPUEvaluator::AddInstruction(const SDFInstruction& instruction) {
    m_instructions.push_back(instruction);

    // Update statistics
    switch (instruction.opcode) {
        case SDFOpcode::PUSH_SPHERE:
        case SDFOpcode::PUSH_BOX:
        case SDFOpcode::PUSH_CAPSULE:
        case SDFOpcode::PUSH_TORUS:
        case SDFOpcode::PUSH_CYLINDER:
        case SDFOpcode::PUSH_CONE:
        case SDFOpcode::PUSH_PLANE:
        case SDFOpcode::PUSH_ELLIPSOID:
            m_stats.primitiveCount++;
            break;

        case SDFOpcode::OP_UNION:
        case SDFOpcode::OP_SUBTRACT:
        case SDFOpcode::OP_INTERSECT:
        case SDFOpcode::OP_SMOOTH_UNION:
        case SDFOpcode::OP_SMOOTH_SUBTRACT:
        case SDFOpcode::OP_SMOOTH_INTERSECT:
            m_stats.operationCount++;
            break;

        case SDFOpcode::TRANSFORM_TRANSLATE:
        case SDFOpcode::TRANSFORM_ROTATE:
        case SDFOpcode::TRANSFORM_SCALE:
        case SDFOpcode::TRANSFORM_MATRIX:
            m_stats.transformCount++;
            break;

        default:
            break;
    }
}

// =============================================================================
// Helper Functions - Primitives
// =============================================================================

void SDFGPUEvaluator::AddSphere(const glm::vec3& center, float radius, uint16_t materialID) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::PUSH_SPHERE;
    inst.materialID = materialID;
    inst.params.sphere.center = center;
    inst.params.sphere.radius = radius;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddBox(const glm::vec3& center, const glm::vec3& halfExtents, uint16_t materialID) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::PUSH_BOX;
    inst.materialID = materialID;
    inst.params.box.center = center;
    inst.params.box.halfExtents = halfExtents;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddCapsule(const glm::vec3& start, const glm::vec3& end, float radius, uint16_t materialID) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::PUSH_CAPSULE;
    inst.materialID = materialID;
    inst.params.capsule.start = start;
    inst.params.capsule.end = end;
    inst.params.capsule.radius = radius;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddTorus(const glm::vec3& center, float majorRadius, float minorRadius, uint16_t materialID) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::PUSH_TORUS;
    inst.materialID = materialID;
    inst.params.torus.center = center;
    inst.params.torus.majorRadius = majorRadius;
    inst.params.torus.minorRadius = minorRadius;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddPlane(const glm::vec3& normal, float distance, uint16_t materialID) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::PUSH_PLANE;
    inst.materialID = materialID;
    inst.params.plane.normal = glm::normalize(normal);
    inst.params.plane.distance = distance;
    AddInstruction(inst);
}

// =============================================================================
// Helper Functions - Operations
// =============================================================================

void SDFGPUEvaluator::AddUnion() {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::OP_UNION;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddSmoothUnion(float smoothness) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::OP_SMOOTH_UNION;
    inst.params.smoothness.k = smoothness;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddSubtract() {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::OP_SUBTRACT;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddIntersect() {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::OP_INTERSECT;
    AddInstruction(inst);
}

// =============================================================================
// Helper Functions - Transforms
// =============================================================================

void SDFGPUEvaluator::AddTranslate(const glm::vec3& offset) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::TRANSFORM_TRANSLATE;
    inst.params.translate.offset = offset;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddRotate(const glm::quat& rotation) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::TRANSFORM_ROTATE;
    inst.params.rotate.rotation = rotation;
    AddInstruction(inst);
}

void SDFGPUEvaluator::AddScale(const glm::vec3& scale) {
    SDFInstruction inst;
    inst.opcode = SDFOpcode::TRANSFORM_SCALE;
    inst.params.scaleOp.scale = scale;
    AddInstruction(inst);
}

// =============================================================================
// Compilation
// =============================================================================

bool SDFGPUEvaluator::Compile() {
    if (!m_initialized) {
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Add RETURN instruction if not present
    if (m_instructions.empty() || m_instructions.back().opcode != SDFOpcode::RETURN) {
        SDFInstruction returnInst;
        returnInst.opcode = SDFOpcode::RETURN;
        m_instructions.push_back(returnInst);
    }

    // Calculate buffer size
    size_t bufferSize = m_instructions.size() * sizeof(SDFInstruction);

    // Upload to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instructionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, m_instructions.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        return false;
    }

    // Update statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.compileTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.totalInstructions = static_cast<uint32_t>(m_instructions.size());
    m_stats.bytecodeSize = bufferSize;

    return true;
}

// =============================================================================
// Evaluation
// =============================================================================

void SDFGPUEvaluator::BindForEvaluation(uint32_t binding) {
    if (!m_initialized) {
        return;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_instructionSSBO);
}

} // namespace Nova
