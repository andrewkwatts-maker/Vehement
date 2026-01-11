#!/usr/bin/env python3

# Add advanced SDF blending functions to header
header_file = "H:/Github/Old3DEngine/engine/sdf/SDFPrimitive.hpp"

with open(header_file, 'r', encoding='utf-8') as f:
    content = f.read()

old_text = """    // CSG operations
    [[nodiscard]] float Union(float d1, float d2);
    [[nodiscard]] float Subtraction(float d1, float d2);
    [[nodiscard]] float Intersection(float d1, float d2);
    [[nodiscard]] float SmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float SmoothSubtraction(float d1, float d2, float k);
    [[nodiscard]] float SmoothIntersection(float d1, float d2, float k);
}

} // namespace Nova"""

new_text = """    // CSG operations
    [[nodiscard]] float Union(float d1, float d2);
    [[nodiscard]] float Subtraction(float d1, float d2);
    [[nodiscard]] float Intersection(float d1, float d2);
    [[nodiscard]] float SmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float SmoothSubtraction(float d1, float d2, float k);
    [[nodiscard]] float SmoothIntersection(float d1, float d2, float k);

    // Advanced smooth blending operations (more organic)
    [[nodiscard]] float ExponentialSmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float PowerSmoothUnion(float d1, float d2, float k);
    [[nodiscard]] float CubicSmoothUnion(float d1, float d2, float k);

    // Distance-aware blending (prevents unwanted self-smoothing during animation)
    [[nodiscard]] float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist);
}

} // namespace Nova"""

if old_text in content:
    content = content.replace(old_text, new_text)
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added advanced SDF blending functions to header")
else:
    print("Could not find insertion point in header")

# Add implementations to cpp file
cpp_file = "H:/Github/Old3DEngine/engine/sdf/SDFPrimitive.cpp"

with open(cpp_file, 'r', encoding='utf-8') as f:
    content = f.read()

old_text = """float SmoothIntersection(float d1, float d2, float k) {
    float h = glm::clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) + k * h * (1.0f - h);
}

} // namespace SDFEval

} // namespace Nova"""

new_text = """float SmoothIntersection(float d1, float d2, float k) {
    float h = glm::clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) + k * h * (1.0f - h);
}

// Exponential smooth union - creates very organic, flowing blends
// More expensive but produces smoother transitions than polynomial
float ExponentialSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float res = std::exp2(-k * d1) + std::exp2(-k * d2);
    return -std::log2(res) / k;
}

// Power smooth union - adjustable blend sharpness via exponent
// k controls blend radius, uses power function for smooth transition
float PowerSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float a = std::pow(d1, k);
    float b = std::pow(d2, k);
    return std::pow((a * b) / (a + b), 1.0f / k);
}

// Cubic smooth union - smoother than quadratic (standard SmoothUnion)
// Produces more organic transitions with better C2 continuity
float CubicSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float h = std::max(k - std::abs(d1 - d2), 0.0f) / k;
    float m = h * h * h * 0.5f;
    float s = m * k * (1.0f / 3.0f);
    return (d1 < d2) ? d1 - s : d2 - s;
}

// Distance-aware smooth union - prevents unwanted blending when parts are far apart
// Critical for character animation to prevent fingers/limbs from merging
float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist) {
    float dist = std::abs(d1 - d2);
    if (dist > minDist) {
        // Too far apart, use hard union
        return std::min(d1, d2);
    }
    // Close enough, apply smooth blending with falloff
    float falloff = 1.0f - (dist / minDist);
    float effectiveK = k * falloff;
    return SmoothUnion(d1, d2, effectiveK);
}

} // namespace SDFEval

} // namespace Nova"""

if old_text in content:
    content = content.replace(old_text, new_text)
    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added advanced SDF blending implementations to cpp")
else:
    print("Could not find insertion point in cpp")
