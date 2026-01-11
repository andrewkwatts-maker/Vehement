#pragma once

#include "ShaderGraph.hpp"

namespace Nova {

// ============================================================================
// NOISE NODES
// ============================================================================

/**
 * @brief Simple value noise
 */
class ValueNoiseNode : public ShaderNode {
public:
    ValueNoiseNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "ValueNoise"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Perlin noise
 */
class PerlinNoiseNode : public ShaderNode {
public:
    PerlinNoiseNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "PerlinNoise"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Simplex noise
 */
class SimplexNoiseNode : public ShaderNode {
public:
    SimplexNoiseNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "SimplexNoise"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Worley/Cellular noise
 */
class WorleyNoiseNode : public ShaderNode {
public:
    WorleyNoiseNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "WorleyNoise"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Voronoi noise
 */
class VoronoiNode : public ShaderNode {
public:
    VoronoiNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "Voronoi"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Fractal Brownian Motion (FBM)
 */
class FBMNode : public ShaderNode {
public:
    FBMNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "FBM"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetOctaves(int octaves) { m_octaves = octaves; }
    void SetLacunarity(float lac) { m_lacunarity = lac; }
    void SetGain(float gain) { m_gain = gain; }

private:
    int m_octaves = 4;
    float m_lacunarity = 2.0f;
    float m_gain = 0.5f;
};

/**
 * @brief Turbulence noise
 */
class TurbulenceNode : public ShaderNode {
public:
    TurbulenceNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "Turbulence"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Gradient/Direction noise
 */
class GradientNoiseNode : public ShaderNode {
public:
    GradientNoiseNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Noise; }
    [[nodiscard]] const char* GetTypeName() const override { return "GradientNoise"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// PATTERN NODES
// ============================================================================

/**
 * @brief Checkerboard pattern
 */
class CheckerboardNode : public ShaderNode {
public:
    CheckerboardNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "Checkerboard"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Brick pattern
 */
class BrickNode : public ShaderNode {
public:
    BrickNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "Brick"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Gradient pattern
 */
class GradientPatternNode : public ShaderNode {
public:
    enum class GradientType { Linear, Radial, Angular, Diamond, Spherical };

    GradientPatternNode(GradientType type = GradientType::Linear);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "Gradient"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetGradientType(GradientType type) { m_type = type; }

private:
    GradientType m_type;
};

/**
 * @brief Polar coordinates conversion
 */
class PolarCoordinatesNode : public ShaderNode {
public:
    PolarCoordinatesNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "PolarCoordinates"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Triplanar mapping
 */
class TriplanarNode : public ShaderNode {
public:
    TriplanarNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "Triplanar"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Tiling and offset
 */
class TilingOffsetNode : public ShaderNode {
public:
    TilingOffsetNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "TilingOffset"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Rotate UV
 */
class RotateUVNode : public ShaderNode {
public:
    RotateUVNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "RotateUV"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Parallax mapping
 */
class ParallaxNode : public ShaderNode {
public:
    ParallaxNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "Parallax"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// SDF NODES (For SDF materials)
// ============================================================================

/**
 * @brief SDF Sphere
 */
class SDFSphereNode : public ShaderNode {
public:
    SDFSphereNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFSphere"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief SDF Box
 */
class SDFBoxNode : public ShaderNode {
public:
    SDFBoxNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFBox"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief SDF Union
 */
class SDFUnionNode : public ShaderNode {
public:
    SDFUnionNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFUnion"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief SDF Subtraction
 */
class SDFSubtractNode : public ShaderNode {
public:
    SDFSubtractNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFSubtract"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief SDF Intersection
 */
class SDFIntersectNode : public ShaderNode {
public:
    SDFIntersectNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFIntersect"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief SDF Smooth Union
 */
class SDFSmoothUnionNode : public ShaderNode {
public:
    SDFSmoothUnionNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Pattern; }
    [[nodiscard]] const char* GetTypeName() const override { return "SDFSmoothUnion"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// GLSL Noise Function Library
// ============================================================================

/**
 * @brief Get all noise function definitions for inclusion in shader
 */
std::string GetNoiseLibraryGLSL();

/**
 * @brief Get SDF function definitions
 */
std::string GetSDFLibraryGLSL();

/**
 * @brief Get color utility function definitions
 */
std::string GetColorLibraryGLSL();

} // namespace Nova
