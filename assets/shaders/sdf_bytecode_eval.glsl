// SDF Bytecode Evaluation - GPU Stack-based VM
// Include file for SDF shaders
// Usage: #include "sdf_bytecode_eval.glsl"

#ifndef SDF_BYTECODE_EVAL_GLSL
#define SDF_BYTECODE_EVAL_GLSL

// =============================================================================
// Opcodes (must match C++ enum)
// =============================================================================

const uint OP_PUSH_SPHERE = 0u;
const uint OP_PUSH_BOX = 1u;
const uint OP_PUSH_CAPSULE = 2u;
const uint OP_PUSH_TORUS = 3u;
const uint OP_PUSH_CYLINDER = 4u;
const uint OP_PUSH_CONE = 5u;
const uint OP_PUSH_PLANE = 6u;
const uint OP_PUSH_ELLIPSOID = 7u;

const uint OP_UNION = 16u;
const uint OP_SUBTRACT = 17u;
const uint OP_INTERSECT = 18u;
const uint OP_SMOOTH_UNION = 19u;
const uint OP_SMOOTH_SUBTRACT = 20u;
const uint OP_SMOOTH_INTERSECT = 21u;

const uint OP_TRANSFORM_TRANSLATE = 32u;
const uint OP_TRANSFORM_ROTATE = 33u;
const uint OP_TRANSFORM_SCALE = 34u;
const uint OP_TRANSFORM_MATRIX = 35u;

const uint OP_MOD_ROUND = 48u;
const uint OP_MOD_ONION = 49u;

const uint OP_DUP = 64u;
const uint OP_SWAP = 65u;
const uint OP_POP = 66u;

const uint OP_SET_MATERIAL = 80u;
const uint OP_RETURN = 255u;

// =============================================================================
// SDF Instruction Structure (must match C++)
// =============================================================================

struct SDFInstruction {
    uint opcode;           // 4 bytes
    uint flags;            // 4 bytes (packed: flags + materialID)
    vec4 param0;           // 16 bytes
    vec4 param1;           // 16 bytes
    vec4 param2;           // 16 bytes
    vec4 param3;           // 16 bytes
    // Total: 64 bytes
};

// =============================================================================
// Primitive SDFs
// =============================================================================

float sdSphere(vec3 p, vec3 center, float radius) {
    return length(p - center) - radius;
}

float sdBox(vec3 p, vec3 center, vec3 halfExtents) {
    vec3 q = abs(p - center) - halfExtents;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float radius) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

float sdTorus(vec3 p, vec3 center, float majorRadius, float minorRadius) {
    vec3 q = p - center;
    vec2 t = vec2(length(q.xz) - majorRadius, q.y);
    return length(t) - minorRadius;
}

float sdCylinder(vec3 p, vec3 a, vec3 b, float radius) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float baba = dot(ba, ba);
    float paba = dot(pa, ba);
    float x = length(pa * baba - ba * paba) - radius * baba;
    float y = abs(paba - baba * 0.5) - baba * 0.5;
    float x2 = x * x;
    float y2 = y * y * baba;
    float d = (max(x, y) < 0.0) ? -min(x2, y2) : (((x > 0.0) ? x2 : 0.0) + ((y > 0.0) ? y2 : 0.0));
    return sign(d) * sqrt(abs(d)) / baba;
}

float sdPlane(vec3 p, vec3 normal, float distance) {
    return dot(p, normal) + distance;
}

// =============================================================================
// CSG Operations
// =============================================================================

float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float opSubtract(float d1, float d2) {
    return max(-d1, d2);
}

float opIntersect(float d1, float d2) {
    return max(d1, d2);
}

float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtract(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersect(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

// =============================================================================
// Stack-based VM for Bytecode Evaluation
// =============================================================================

const int MAX_STACK_SIZE = 32;

struct SDFStack {
    float distances[MAX_STACK_SIZE];
    uint materials[MAX_STACK_SIZE];
    int sp;  // Stack pointer
};

void stackPush(inout SDFStack stack, float distance, uint material) {
    if (stack.sp < MAX_STACK_SIZE) {
        stack.distances[stack.sp] = distance;
        stack.materials[stack.sp] = material;
        stack.sp++;
    }
}

void stackPop(inout SDFStack stack, out float distance, out uint material) {
    if (stack.sp > 0) {
        stack.sp--;
        distance = stack.distances[stack.sp];
        material = stack.materials[stack.sp];
    } else {
        distance = 1000.0;
        material = 0u;
    }
}

void stackPeek(in SDFStack stack, out float distance, out uint material) {
    if (stack.sp > 0) {
        distance = stack.distances[stack.sp - 1];
        material = stack.materials[stack.sp - 1];
    } else {
        distance = 1000.0;
        material = 0u;
    }
}

// =============================================================================
// Bytecode Evaluator
// =============================================================================

// Evaluate SDF bytecode at position p
// Returns: signed distance
// Out parameter: materialID
float evaluateSDFBytecode(
    vec3 p,
    out uint materialID,
    SDFInstruction instructions[],
    int instructionCount
) {
    SDFStack stack;
    stack.sp = 0;

    vec3 currentPos = p;
    uint currentMaterial = 0u;

    for (int pc = 0; pc < instructionCount; pc++) {
        SDFInstruction inst = instructions[pc];
        uint opcode = inst.opcode & 0xFFu;
        uint instMaterial = (inst.flags >> 16u) & 0xFFFFu;

        if (opcode == OP_PUSH_SPHERE) {
            vec3 center = inst.param0.xyz;
            float radius = inst.param0.w;
            float dist = sdSphere(currentPos, center, radius);
            stackPush(stack, dist, instMaterial);
        }
        else if (opcode == OP_PUSH_BOX) {
            vec3 center = inst.param0.xyz;
            vec3 halfExtents = inst.param1.xyz;
            float dist = sdBox(currentPos, center, halfExtents);
            stackPush(stack, dist, instMaterial);
        }
        else if (opcode == OP_PUSH_CAPSULE) {
            vec3 start = inst.param0.xyz;
            vec3 end = inst.param1.xyz;
            float radius = inst.param1.w;
            float dist = sdCapsule(currentPos, start, end, radius);
            stackPush(stack, dist, instMaterial);
        }
        else if (opcode == OP_PUSH_TORUS) {
            vec3 center = inst.param0.xyz;
            float majorRadius = inst.param0.w;
            float minorRadius = inst.param1.x;
            float dist = sdTorus(currentPos, center, majorRadius, minorRadius);
            stackPush(stack, dist, instMaterial);
        }
        else if (opcode == OP_PUSH_PLANE) {
            vec3 normal = inst.param0.xyz;
            float distance = inst.param0.w;
            float dist = sdPlane(currentPos, normal, distance);
            stackPush(stack, dist, instMaterial);
        }
        else if (opcode == OP_UNION) {
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opUnion(d1, d2);
            uint resultMat = (d1 < d2) ? m1 : m2;
            stackPush(stack, result, resultMat);
        }
        else if (opcode == OP_SUBTRACT) {
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opSubtract(d1, d2);
            stackPush(stack, result, m2);
        }
        else if (opcode == OP_INTERSECT) {
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opIntersect(d1, d2);
            uint resultMat = (d1 > d2) ? m1 : m2;
            stackPush(stack, result, resultMat);
        }
        else if (opcode == OP_SMOOTH_UNION) {
            float k = inst.param0.x;
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opSmoothUnion(d1, d2, k);
            uint resultMat = (d1 < d2) ? m1 : m2;
            stackPush(stack, result, resultMat);
        }
        else if (opcode == OP_SMOOTH_SUBTRACT) {
            float k = inst.param0.x;
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opSmoothSubtract(d1, d2, k);
            stackPush(stack, result, m2);
        }
        else if (opcode == OP_SMOOTH_INTERSECT) {
            float k = inst.param0.x;
            float d1, d2;
            uint m1, m2;
            stackPop(stack, d2, m2);
            stackPop(stack, d1, m1);
            float result = opSmoothIntersect(d1, d2, k);
            uint resultMat = (d1 > d2) ? m1 : m2;
            stackPush(stack, result, resultMat);
        }
        else if (opcode == OP_RETURN) {
            break;
        }
    }

    // Return top of stack
    float finalDist;
    stackPop(stack, finalDist, materialID);

    return finalDist;
}

#endif // SDF_BYTECODE_EVAL_GLSL
