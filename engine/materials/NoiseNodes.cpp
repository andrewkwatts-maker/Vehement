/**
 * @file NoiseNodes.cpp
 * @brief Implementation of noise, pattern, and SDF shader nodes
 */

#include "NoiseNodes.hpp"

namespace Nova {

// ============================================================================
// NOISE NODES
// ============================================================================

ValueNoiseNode::ValueNoiseNode() : ShaderNode("ValueNoise") {
    m_displayName = "Value Noise";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Scale", 1.0f);
}

std::string ValueNoiseNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "valueNoise");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = valueNoise(" + uvVar + " * " + scaleVar + ");";
}

PerlinNoiseNode::PerlinNoiseNode() : ShaderNode("PerlinNoise") {
    m_displayName = "Perlin Noise";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Scale", 1.0f);
}

std::string PerlinNoiseNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "perlinNoise");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = perlinNoise(" + uvVar + " * " + scaleVar + ");";
}

SimplexNoiseNode::SimplexNoiseNode() : ShaderNode("SimplexNoise") {
    m_displayName = "Simplex Noise";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Scale", 1.0f);
}

std::string SimplexNoiseNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "simplexNoise");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = simplexNoise(" + uvVar + " * " + scaleVar + ");";
}

WorleyNoiseNode::WorleyNoiseNode() : ShaderNode("WorleyNoise") {
    m_displayName = "Worley Noise";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddInput("Jitter", ShaderDataType::Float, "Jitter");
    AddOutput("F1", ShaderDataType::Float, "F1");
    AddOutput("F2", ShaderDataType::Float, "F2");
    AddOutput("CellID", ShaderDataType::Vec2, "CellID");
    SetInputDefault("Scale", 1.0f);
    SetInputDefault("Jitter", 1.0f);
}

std::string WorleyNoiseNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string jitterVar = GetInputValue("Jitter", compiler);

    std::string f1Var = compiler.AllocateVariable(ShaderDataType::Float, "worleyF1");
    std::string f2Var = compiler.AllocateVariable(ShaderDataType::Float, "worleyF2");
    std::string cellVar = compiler.AllocateVariable(ShaderDataType::Vec2, "worleyCell");
    std::string resultVar = compiler.AllocateVariable(ShaderDataType::Vec4, "worleyResult");

    compiler.SetNodeOutputVariable(m_id, "F1", f1Var);
    compiler.SetNodeOutputVariable(m_id, "F2", f2Var);
    compiler.SetNodeOutputVariable(m_id, "CellID", cellVar);

    return "vec4 " + resultVar + " = worleyNoise(" + uvVar + " * " + scaleVar + ", " + jitterVar + ");\n"
           "float " + f1Var + " = " + resultVar + ".x;\n"
           "float " + f2Var + " = " + resultVar + ".y;\n"
           "vec2 " + cellVar + " = " + resultVar + ".zw;";
}

VoronoiNode::VoronoiNode() : ShaderNode("Voronoi") {
    m_displayName = "Voronoi";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddInput("AngleOffset", ShaderDataType::Float, "Angle Offset");
    AddOutput("Cells", ShaderDataType::Float, "Cells");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
    AddOutput("CellPosition", ShaderDataType::Vec2, "Cell Position");
    SetInputDefault("Scale", 5.0f);
    SetInputDefault("AngleOffset", 0.0f);
}

std::string VoronoiNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string angleVar = GetInputValue("AngleOffset", compiler);

    std::string cellsVar = compiler.AllocateVariable(ShaderDataType::Float, "voronoiCells");
    std::string distVar = compiler.AllocateVariable(ShaderDataType::Float, "voronoiDist");
    std::string posVar = compiler.AllocateVariable(ShaderDataType::Vec2, "voronoiPos");
    std::string resultVar = compiler.AllocateVariable(ShaderDataType::Vec4, "voronoiResult");

    compiler.SetNodeOutputVariable(m_id, "Cells", cellsVar);
    compiler.SetNodeOutputVariable(m_id, "Distance", distVar);
    compiler.SetNodeOutputVariable(m_id, "CellPosition", posVar);

    return "vec4 " + resultVar + " = voronoiNoise(" + uvVar + " * " + scaleVar + ", " + angleVar + ");\n"
           "float " + cellsVar + " = " + resultVar + ".x;\n"
           "float " + distVar + " = " + resultVar + ".y;\n"
           "vec2 " + posVar + " = " + resultVar + ".zw;";
}

FBMNode::FBMNode() : ShaderNode("FBM") {
    m_displayName = "FBM";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddInput("Octaves", ShaderDataType::Int, "Octaves");
    AddInput("Lacunarity", ShaderDataType::Float, "Lacunarity");
    AddInput("Gain", ShaderDataType::Float, "Gain");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Scale", 1.0f);
    SetInputDefault("Octaves", 4);
    SetInputDefault("Lacunarity", 2.0f);
    SetInputDefault("Gain", 0.5f);
}

std::string FBMNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string octavesVar = GetInputValue("Octaves", compiler);
    std::string lacunarityVar = GetInputValue("Lacunarity", compiler);
    std::string gainVar = GetInputValue("Gain", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "fbm");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = fbmNoise(" + uvVar + " * " + scaleVar + ", " +
           octavesVar + ", " + lacunarityVar + ", " + gainVar + ");";
}

TurbulenceNode::TurbulenceNode() : ShaderNode("Turbulence") {
    m_displayName = "Turbulence";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddInput("Octaves", ShaderDataType::Int, "Octaves");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Scale", 1.0f);
    SetInputDefault("Octaves", 4);
}

std::string TurbulenceNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string octavesVar = GetInputValue("Octaves", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "turbulence");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = turbulenceNoise(" + uvVar + " * " + scaleVar + ", " + octavesVar + ");";
}

GradientNoiseNode::GradientNoiseNode() : ShaderNode("GradientNoise") {
    m_displayName = "Gradient Noise";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddOutput("Value", ShaderDataType::Float, "Value");
    AddOutput("Direction", ShaderDataType::Vec2, "Direction");
    SetInputDefault("Scale", 1.0f);
}

std::string GradientNoiseNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "gradNoise");
    std::string dirVar = compiler.AllocateVariable(ShaderDataType::Vec2, "gradDir");
    std::string resultVar = compiler.AllocateVariable(ShaderDataType::Vec3, "gradResult");

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    compiler.SetNodeOutputVariable(m_id, "Direction", dirVar);

    return "vec3 " + resultVar + " = gradientNoise(" + uvVar + " * " + scaleVar + ");\n"
           "float " + outVar + " = " + resultVar + ".x;\n"
           "vec2 " + dirVar + " = " + resultVar + ".yz;";
}

// ============================================================================
// PATTERN NODES
// ============================================================================

CheckerboardNode::CheckerboardNode() : ShaderNode("Checkerboard") {
    m_displayName = "Checkerboard";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Scale", ShaderDataType::Vec2, "Scale");
    AddInput("ColorA", ShaderDataType::Vec3, "Color A");
    AddInput("ColorB", ShaderDataType::Vec3, "Color B");
    AddOutput("Color", ShaderDataType::Vec3, "Color");
    AddOutput("Mask", ShaderDataType::Float, "Mask");
    SetInputDefault("Scale", glm::vec2(1.0f));
    SetInputDefault("ColorA", glm::vec3(0.0f));
    SetInputDefault("ColorB", glm::vec3(1.0f));
}

std::string CheckerboardNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string colorAVar = GetInputValue("ColorA", compiler);
    std::string colorBVar = GetInputValue("ColorB", compiler);

    std::string maskVar = compiler.AllocateVariable(ShaderDataType::Float, "checkerMask");
    std::string colorVar = compiler.AllocateVariable(ShaderDataType::Vec3, "checkerColor");
    std::string checkerUV = compiler.AllocateVariable(ShaderDataType::Vec2, "checkerUV");

    compiler.SetNodeOutputVariable(m_id, "Color", colorVar);
    compiler.SetNodeOutputVariable(m_id, "Mask", maskVar);

    return "vec2 " + checkerUV + " = floor(" + uvVar + " * " + scaleVar + ");\n"
           "float " + maskVar + " = mod(" + checkerUV + ".x + " + checkerUV + ".y, 2.0);\n"
           "vec3 " + colorVar + " = mix(" + colorAVar + ", " + colorBVar + ", " + maskVar + ");";
}

BrickNode::BrickNode() : ShaderNode("Brick") {
    m_displayName = "Brick";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("BrickSize", ShaderDataType::Vec2, "Brick Size");
    AddInput("MortarSize", ShaderDataType::Float, "Mortar Size");
    AddInput("BrickOffset", ShaderDataType::Float, "Brick Offset");
    AddOutput("Color", ShaderDataType::Float, "Color");
    AddOutput("BrickID", ShaderDataType::Vec2, "Brick ID");
    SetInputDefault("BrickSize", glm::vec2(3.0f, 1.0f));
    SetInputDefault("MortarSize", 0.05f);
    SetInputDefault("BrickOffset", 0.5f);
}

std::string BrickNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string sizeVar = GetInputValue("BrickSize", compiler);
    std::string mortarVar = GetInputValue("MortarSize", compiler);
    std::string offsetVar = GetInputValue("BrickOffset", compiler);

    std::string colorVar = compiler.AllocateVariable(ShaderDataType::Float, "brickColor");
    std::string idVar = compiler.AllocateVariable(ShaderDataType::Vec2, "brickID");
    std::string resultVar = compiler.AllocateVariable(ShaderDataType::Vec4, "brickResult");

    compiler.SetNodeOutputVariable(m_id, "Color", colorVar);
    compiler.SetNodeOutputVariable(m_id, "BrickID", idVar);

    return "vec4 " + resultVar + " = brickPattern(" + uvVar + ", " + sizeVar + ", " + mortarVar + ", " + offsetVar + ");\n"
           "float " + colorVar + " = " + resultVar + ".x;\n"
           "vec2 " + idVar + " = " + resultVar + ".zw;";
}

GradientPatternNode::GradientPatternNode(GradientType type) : ShaderNode("Gradient"), m_type(type) {
    m_displayName = "Gradient";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Center", ShaderDataType::Vec2, "Center");
    AddInput("Rotation", ShaderDataType::Float, "Rotation");
    AddOutput("Value", ShaderDataType::Float, "Value");
    SetInputDefault("Center", glm::vec2(0.5f));
    SetInputDefault("Rotation", 0.0f);
}

std::string GradientPatternNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string centerVar = GetInputValue("Center", compiler);
    std::string rotVar = GetInputValue("Rotation", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "gradient");

    std::string funcName;
    switch (m_type) {
        case GradientType::Linear: funcName = "linearGradient"; break;
        case GradientType::Radial: funcName = "radialGradient"; break;
        case GradientType::Angular: funcName = "angularGradient"; break;
        case GradientType::Diamond: funcName = "diamondGradient"; break;
        case GradientType::Spherical: funcName = "sphericalGradient"; break;
    }

    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return "float " + outVar + " = " + funcName + "(" + uvVar + ", " + centerVar + ", " + rotVar + ");";
}

PolarCoordinatesNode::PolarCoordinatesNode() : ShaderNode("PolarCoordinates") {
    m_displayName = "Polar Coordinates";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Center", ShaderDataType::Vec2, "Center");
    AddInput("RadialScale", ShaderDataType::Float, "Radial Scale");
    AddInput("LengthScale", ShaderDataType::Float, "Length Scale");
    AddOutput("Polar", ShaderDataType::Vec2, "Polar");
    SetInputDefault("Center", glm::vec2(0.5f));
    SetInputDefault("RadialScale", 1.0f);
    SetInputDefault("LengthScale", 1.0f);
}

std::string PolarCoordinatesNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string centerVar = GetInputValue("Center", compiler);
    std::string radialVar = GetInputValue("RadialScale", compiler);
    std::string lengthVar = GetInputValue("LengthScale", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Vec2, "polar");
    std::string deltaVar = compiler.AllocateVariable(ShaderDataType::Vec2, "delta");

    compiler.SetNodeOutputVariable(m_id, "Polar", outVar);

    return "vec2 " + deltaVar + " = " + uvVar + " - " + centerVar + ";\n"
           "float _radius = length(" + deltaVar + ") * 2.0 * " + lengthVar + ";\n"
           "float _angle = atan(" + deltaVar + ".y, " + deltaVar + ".x) * " + radialVar + " / 3.14159265;\n"
           "vec2 " + outVar + " = vec2(_radius, _angle);";
}

TriplanarNode::TriplanarNode() : ShaderNode("Triplanar") {
    m_displayName = "Triplanar";
    AddInput("Texture", ShaderDataType::Sampler2D, "Texture");
    AddInput("Position", ShaderDataType::Vec3, "Position");
    AddInput("Normal", ShaderDataType::Vec3, "Normal");
    AddInput("Scale", ShaderDataType::Float, "Scale");
    AddInput("Sharpness", ShaderDataType::Float, "Sharpness");
    AddOutput("Color", ShaderDataType::Vec4, "Color");
    SetInputDefault("Scale", 1.0f);
    SetInputDefault("Sharpness", 1.0f);
}

std::string TriplanarNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string texVar = GetInputValue("Texture", compiler);
    std::string posVar = GetInputValue("Position", compiler);
    std::string normalVar = GetInputValue("Normal", compiler);
    std::string scaleVar = GetInputValue("Scale", compiler);
    std::string sharpVar = GetInputValue("Sharpness", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Vec4, "triplanar");

    compiler.SetNodeOutputVariable(m_id, "Color", outVar);
    return "vec4 " + outVar + " = triplanarSample(" + texVar + ", " + posVar + ", " + normalVar + ", " + scaleVar + ", " + sharpVar + ");";
}

TilingOffsetNode::TilingOffsetNode() : ShaderNode("TilingOffset") {
    m_displayName = "Tiling Offset";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Tiling", ShaderDataType::Vec2, "Tiling");
    AddInput("Offset", ShaderDataType::Vec2, "Offset");
    AddOutput("UV", ShaderDataType::Vec2, "UV");
    SetInputDefault("Tiling", glm::vec2(1.0f));
    SetInputDefault("Offset", glm::vec2(0.0f));
}

std::string TilingOffsetNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string tilingVar = GetInputValue("Tiling", compiler);
    std::string offsetVar = GetInputValue("Offset", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Vec2, "tiledUV");

    compiler.SetNodeOutputVariable(m_id, "UV", outVar);
    return "vec2 " + outVar + " = " + uvVar + " * " + tilingVar + " + " + offsetVar + ";";
}

RotateUVNode::RotateUVNode() : ShaderNode("RotateUV") {
    m_displayName = "Rotate UV";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Center", ShaderDataType::Vec2, "Center");
    AddInput("Rotation", ShaderDataType::Float, "Rotation");
    AddOutput("UV", ShaderDataType::Vec2, "UV");
    SetInputDefault("Center", glm::vec2(0.5f));
    SetInputDefault("Rotation", 0.0f);
}

std::string RotateUVNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = GetInputValue("UV", compiler);
    std::string centerVar = GetInputValue("Center", compiler);
    std::string rotVar = GetInputValue("Rotation", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Vec2, "rotatedUV");

    compiler.SetNodeOutputVariable(m_id, "UV", outVar);

    return "float _rotSin = sin(" + rotVar + ");\n"
           "float _rotCos = cos(" + rotVar + ");\n"
           "vec2 _centeredUV = " + uvVar + " - " + centerVar + ";\n"
           "vec2 " + outVar + " = vec2(_centeredUV.x * _rotCos - _centeredUV.y * _rotSin, _centeredUV.x * _rotSin + _centeredUV.y * _rotCos) + " + centerVar + ";";
}

ParallaxNode::ParallaxNode() : ShaderNode("Parallax") {
    m_displayName = "Parallax";
    AddInput("HeightMap", ShaderDataType::Sampler2D, "Height Map");
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("ViewDir", ShaderDataType::Vec3, "View Direction");
    AddInput("Height", ShaderDataType::Float, "Height");
    AddInput("Steps", ShaderDataType::Int, "Steps");
    AddOutput("UV", ShaderDataType::Vec2, "UV");
    AddOutput("Height", ShaderDataType::Float, "Height");
    SetInputDefault("Height", 0.05f);
    SetInputDefault("Steps", 8);
}

std::string ParallaxNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string texVar = GetInputValue("HeightMap", compiler);
    std::string uvVar = GetInputValue("UV", compiler);
    std::string viewVar = GetInputValue("ViewDir", compiler);
    std::string heightVar = GetInputValue("Height", compiler);
    std::string stepsVar = GetInputValue("Steps", compiler);

    std::string outUVVar = compiler.AllocateVariable(ShaderDataType::Vec2, "parallaxUV");
    std::string outHeightVar = compiler.AllocateVariable(ShaderDataType::Float, "parallaxHeight");
    std::string resultVar = compiler.AllocateVariable(ShaderDataType::Vec3, "parallaxResult");

    compiler.SetNodeOutputVariable(m_id, "UV", outUVVar);
    compiler.SetNodeOutputVariable(m_id, "Height", outHeightVar);

    return "vec3 " + resultVar + " = parallaxMapping(" + texVar + ", " + uvVar + ", " + viewVar + ", " + heightVar + ", " + stepsVar + ");\n"
           "vec2 " + outUVVar + " = " + resultVar + ".xy;\n"
           "float " + outHeightVar + " = " + resultVar + ".z;";
}

// ============================================================================
// SDF NODES
// ============================================================================

SDFSphereNode::SDFSphereNode() : ShaderNode("SDFSphere") {
    m_displayName = "SDF Sphere";
    AddInput("Position", ShaderDataType::Vec3, "Position");
    AddInput("Center", ShaderDataType::Vec3, "Center");
    AddInput("Radius", ShaderDataType::Float, "Radius");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
    SetInputDefault("Center", glm::vec3(0.0f));
    SetInputDefault("Radius", 1.0f);
}

std::string SDFSphereNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string posVar = GetInputValue("Position", compiler);
    std::string centerVar = GetInputValue("Center", compiler);
    std::string radiusVar = GetInputValue("Radius", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfSphere");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);
    return "float " + outVar + " = length(" + posVar + " - " + centerVar + ") - " + radiusVar + ";";
}

SDFBoxNode::SDFBoxNode() : ShaderNode("SDFBox") {
    m_displayName = "SDF Box";
    AddInput("Position", ShaderDataType::Vec3, "Position");
    AddInput("Center", ShaderDataType::Vec3, "Center");
    AddInput("Size", ShaderDataType::Vec3, "Size");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
    SetInputDefault("Center", glm::vec3(0.0f));
    SetInputDefault("Size", glm::vec3(1.0f));
}

std::string SDFBoxNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string posVar = GetInputValue("Position", compiler);
    std::string centerVar = GetInputValue("Center", compiler);
    std::string sizeVar = GetInputValue("Size", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfBox");
    std::string deltaVar = compiler.AllocateVariable(ShaderDataType::Vec3, "boxDelta");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);

    return "vec3 " + deltaVar + " = abs(" + posVar + " - " + centerVar + ") - " + sizeVar + " * 0.5;\n"
           "float " + outVar + " = length(max(" + deltaVar + ", 0.0)) + min(max(" + deltaVar + ".x, max(" + deltaVar + ".y, " + deltaVar + ".z)), 0.0);";
}

SDFUnionNode::SDFUnionNode() : ShaderNode("SDFUnion") {
    m_displayName = "SDF Union";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
}

std::string SDFUnionNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string aVar = GetInputValue("A", compiler);
    std::string bVar = GetInputValue("B", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfUnion");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);
    return "float " + outVar + " = min(" + aVar + ", " + bVar + ");";
}

SDFSubtractNode::SDFSubtractNode() : ShaderNode("SDFSubtract") {
    m_displayName = "SDF Subtract";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
}

std::string SDFSubtractNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string aVar = GetInputValue("A", compiler);
    std::string bVar = GetInputValue("B", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfSubtract");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);
    return "float " + outVar + " = max(" + aVar + ", -" + bVar + ");";
}

SDFIntersectNode::SDFIntersectNode() : ShaderNode("SDFIntersect") {
    m_displayName = "SDF Intersect";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
}

std::string SDFIntersectNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string aVar = GetInputValue("A", compiler);
    std::string bVar = GetInputValue("B", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfIntersect");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);
    return "float " + outVar + " = max(" + aVar + ", " + bVar + ");";
}

SDFSmoothUnionNode::SDFSmoothUnionNode() : ShaderNode("SDFSmoothUnion") {
    m_displayName = "SDF Smooth Union";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddInput("Smoothness", ShaderDataType::Float, "Smoothness");
    AddOutput("Distance", ShaderDataType::Float, "Distance");
    SetInputDefault("Smoothness", 0.1f);
}

std::string SDFSmoothUnionNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string aVar = GetInputValue("A", compiler);
    std::string bVar = GetInputValue("B", compiler);
    std::string kVar = GetInputValue("Smoothness", compiler);
    std::string outVar = compiler.AllocateVariable(ShaderDataType::Float, "sdfSmoothUnion");

    compiler.SetNodeOutputVariable(m_id, "Distance", outVar);

    return "float _smoothH = clamp(0.5 + 0.5 * (" + bVar + " - " + aVar + ") / " + kVar + ", 0.0, 1.0);\n"
           "float " + outVar + " = mix(" + bVar + ", " + aVar + ", _smoothH) - " + kVar + " * _smoothH * (1.0 - _smoothH);";
}

// ============================================================================
// GLSL LIBRARY FUNCTIONS
// ============================================================================

std::string GetNoiseLibraryGLSL() {
    return R"(
// Hash functions for noise
vec2 hash22(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

vec3 hash33(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Value noise
float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Perlin noise
float perlinNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(dot(hash22(i), f),
                   dot(hash22(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
               mix(dot(hash22(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                   dot(hash22(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

// Simplex noise
float simplexNoise(vec2 p) {
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;

    vec2 i = floor(p + (p.x + p.y) * K1);
    vec2 a = p - i + (i.x + i.y) * K2;
    float m = step(a.y, a.x);
    vec2 o = vec2(m, 1.0 - m);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0 * K2;

    vec3 h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    vec3 n = h * h * h * h * vec3(dot(a, hash22(i)), dot(b, hash22(i + o)), dot(c, hash22(i + 1.0)));

    return dot(n, vec3(70.0));
}

// Worley noise
vec4 worleyNoise(vec2 p, float jitter) {
    vec2 n = floor(p);
    vec2 f = fract(p);

    float f1 = 8.0;
    float f2 = 8.0;
    vec2 cellId = vec2(0.0);

    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash22(n + g) * jitter;
            vec2 r = g - f + (0.5 + 0.5 * o);
            float d = dot(r, r);

            if (d < f1) {
                f2 = f1;
                f1 = d;
                cellId = n + g;
            } else if (d < f2) {
                f2 = d;
            }
        }
    }

    return vec4(sqrt(f1), sqrt(f2), cellId);
}

// Voronoi noise
vec4 voronoiNoise(vec2 p, float angleOffset) {
    vec2 n = floor(p);
    vec2 f = fract(p);

    float md = 8.0;
    vec2 mr = vec2(0.0);
    vec2 mg = vec2(0.0);

    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash22(n + g);
            o = 0.5 + 0.5 * sin(angleOffset + 6.2831 * o);
            vec2 r = g + o - f;
            float d = dot(r, r);

            if (d < md) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    float cellId = hash21(n + mg);
    return vec4(cellId, sqrt(md), mr);
}

// FBM noise
float fbmNoise(vec2 p, int octaves, float lacunarity, float gain) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * perlinNoise(p * frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

// Turbulence noise
float turbulenceNoise(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * abs(perlinNoise(p * frequency));
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}

// Gradient noise with direction
vec3 gradientNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    vec2 u = f * f * (3.0 - 2.0 * f);
    vec2 du = 6.0 * f * (1.0 - f);

    vec2 ga = hash22(i);
    vec2 gb = hash22(i + vec2(1.0, 0.0));
    vec2 gc = hash22(i + vec2(0.0, 1.0));
    vec2 gd = hash22(i + vec2(1.0, 1.0));

    float va = dot(ga, f);
    float vb = dot(gb, f - vec2(1.0, 0.0));
    float vc = dot(gc, f - vec2(0.0, 1.0));
    float vd = dot(gd, f - vec2(1.0, 1.0));

    float value = va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd);
    vec2 deriv = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +
                 du * (u.yx * (va - vb - vc + vd) + vec2(vb, vc) - va);

    return vec3(value, deriv);
}
)";
}

std::string GetSDFLibraryGLSL() {
    return R"(
// SDF Primitives
float sdfSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdfBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdfCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdfCone(vec3 p, vec2 c, float h) {
    vec2 q = h * vec2(c.x / c.y, -1.0);
    vec2 w = vec2(length(p.xz), p.y);
    vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
    vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
    float k = sign(q.y);
    float d = min(dot(a, a), dot(b, b));
    float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
    return sqrt(d) * sign(s);
}

float sdfTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

// SDF Operations
float sdfUnion(float d1, float d2) {
    return min(d1, d2);
}

float sdfSubtract(float d1, float d2) {
    return max(d1, -d2);
}

float sdfIntersect(float d1, float d2) {
    return max(d1, d2);
}

float sdfSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float sdfSmoothSubtract(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d1, -d2, h) + k * h * (1.0 - h);
}

float sdfSmoothIntersect(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

float sdfRound(float d, float r) {
    return d - r;
}

float sdfOnion(float d, float thickness) {
    return abs(d) - thickness;
}
)";
}

std::string GetColorLibraryGLSL() {
    return R"(
// Color space conversions
vec3 rgbToHsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsvToRgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Blend modes
vec3 blendNormal(vec3 base, vec3 blend) { return blend; }
vec3 blendMultiply(vec3 base, vec3 blend) { return base * blend; }
vec3 blendScreen(vec3 base, vec3 blend) { return 1.0 - (1.0 - base) * (1.0 - blend); }
vec3 blendOverlay(vec3 base, vec3 blend) {
    return mix(2.0 * base * blend, 1.0 - 2.0 * (1.0 - base) * (1.0 - blend), step(0.5, base));
}
vec3 blendSoftLight(vec3 base, vec3 blend) {
    return mix(2.0 * base * blend + base * base * (1.0 - 2.0 * blend),
               sqrt(base) * (2.0 * blend - 1.0) + 2.0 * base * (1.0 - blend),
               step(0.5, blend));
}
vec3 blendHardLight(vec3 base, vec3 blend) { return blendOverlay(blend, base); }
vec3 blendDifference(vec3 base, vec3 blend) { return abs(base - blend); }
vec3 blendExclusion(vec3 base, vec3 blend) { return base + blend - 2.0 * base * blend; }

// Gradient patterns
float linearGradient(vec2 uv, vec2 center, float rotation) {
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 d = uv - center;
    return d.x * c + d.y * s + 0.5;
}

float radialGradient(vec2 uv, vec2 center, float rotation) {
    return length(uv - center) * 2.0;
}

float angularGradient(vec2 uv, vec2 center, float rotation) {
    vec2 d = uv - center;
    return (atan(d.y, d.x) + rotation) / 6.28318530718 + 0.5;
}

float diamondGradient(vec2 uv, vec2 center, float rotation) {
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 d = uv - center;
    vec2 rd = vec2(d.x * c - d.y * s, d.x * s + d.y * c);
    return (abs(rd.x) + abs(rd.y)) * 2.0;
}

float sphericalGradient(vec2 uv, vec2 center, float rotation) {
    float d = length(uv - center) * 2.0;
    return sqrt(max(1.0 - d * d, 0.0));
}

// Brick pattern
vec4 brickPattern(vec2 uv, vec2 brickSize, float mortarSize, float brickOffset) {
    vec2 brickUV = uv * brickSize;
    float row = floor(brickUV.y);
    brickUV.x += mod(row, 2.0) * brickOffset;
    vec2 brick = fract(brickUV);
    vec2 brickId = floor(brickUV);

    float mortarX = step(brick.x, mortarSize) + step(1.0 - mortarSize, brick.x);
    float mortarY = step(brick.y, mortarSize) + step(1.0 - mortarSize, brick.y);
    float mortar = max(mortarX, mortarY);

    return vec4(1.0 - mortar, brick, brickId);
}

// Triplanar sampling
vec4 triplanarSample(sampler2D tex, vec3 worldPos, vec3 worldNormal, float scale, float sharpness) {
    vec3 blending = pow(abs(worldNormal), vec3(sharpness));
    blending = blending / (blending.x + blending.y + blending.z);

    vec4 xAxis = texture(tex, worldPos.yz * scale);
    vec4 yAxis = texture(tex, worldPos.xz * scale);
    vec4 zAxis = texture(tex, worldPos.xy * scale);

    return xAxis * blending.x + yAxis * blending.y + zAxis * blending.z;
}

// Parallax mapping
vec3 parallaxMapping(sampler2D heightMap, vec2 uv, vec3 viewDir, float height, int steps) {
    float stepSize = 1.0 / float(steps);
    float currentDepth = 0.0;
    vec2 deltaUV = viewDir.xy * height / float(steps);
    vec2 currentUV = uv;
    float currentHeight = texture(heightMap, currentUV).r;

    for (int i = 0; i < steps; i++) {
        if (currentDepth >= currentHeight) break;
        currentUV -= deltaUV;
        currentHeight = texture(heightMap, currentUV).r;
        currentDepth += stepSize;
    }

    vec2 prevUV = currentUV + deltaUV;
    float afterDepth = currentHeight - currentDepth;
    float beforeDepth = texture(heightMap, prevUV).r - currentDepth + stepSize;
    float weight = afterDepth / (afterDepth - beforeDepth);

    return vec3(mix(currentUV, prevUV, weight), currentHeight);
}
)";
}

} // namespace Nova
