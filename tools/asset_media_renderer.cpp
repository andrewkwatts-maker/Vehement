/**
 * @file asset_media_renderer.cpp
 * @brief Standalone tool to render SDF assets to PNG (static) or MP4 (animated)
 *
 * Usage:
 *   asset_media_renderer <asset.json> <output.png|output.mp4> [width] [height] [--fps 30] [--duration 3.0]
 *
 * Automatically detects:
 * - Static assets → PNG icon
 * - Animated assets → MP4 video (or GIF)
 * - Units/buildings → Use idle animation if available
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <cstdlib>
#include <vector>
#include <string>
#include <cstring>

// OpenGL for framebuffer reading
#include <glad/gl.h>

// GLFW for window/context creation
#include <GLFW/glfw3.h>

// STB Image Write for PNG output (implementation is in nova3d library)
#include <stb_image_write.h>

// GLM for math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// JSON parsing
#include <nlohmann/json.hpp>

// Engine includes
#include "../engine/core/Logger.hpp"
#include "../engine/graphics/Framebuffer.hpp"
#include "../engine/graphics/SDFRenderer.hpp"
#include "../engine/sdf/SDFModel.hpp"
#include "../engine/sdf/SDFPrimitive.hpp"
#include "../engine/sdf/SDFAnimation.hpp"
#include "../engine/scene/Camera.hpp"
#include "../engine/graphics/Shader.hpp"
#include "../engine/core/Window.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Forward declarations
bool SaveFramebufferToPNG(const Nova::Framebuffer& fb, const std::string& outputPath);

/**
 * @brief Render configuration
 */
struct RenderConfig {
    std::string assetPath;
    std::string outputPath;
    int width = 512;
    int height = 512;
    int fps = 30;
    float duration = 3.0f;  // seconds
    bool forceStatic = false;
    bool forceAnimation = false;
    std::string animationName = "idle";  // Default animation to use

    // Validation/debug modes
    bool render6Views = false;       // Render all 6 orthographic views
    bool debugColors = false;        // Per-primitive coloring for debugging
    bool validateShadows = false;    // Force shadows on for validation
    bool validateGI = false;         // Force global illumination validation
    bool highQuality = false;        // AAA quality settings
};

/**
 * @brief Asset type detection
 */
enum class AssetType {
    Static,      // No animation
    Animated,    // Has animations
    Unit,        // Unit with idle animation
    Building     // Building with idle animation
};

/**
 * @brief Initialize minimal OpenGL context
 */
class OffscreenContext {
public:
    OffscreenContext(int width, int height)
        : m_width(width), m_height(height) {

        // Create hidden window for OpenGL context
        Nova::Window::CreateParams params;
        params.title = "Asset Media Renderer";
        params.width = width;
        params.height = height;
        params.vsync = false;
        params.visible = false;  // Hidden for batch processing

        m_window = std::make_unique<Nova::Window>();
        if (!m_window->Create(params)) {
            throw std::runtime_error("Failed to create OpenGL context");
        }

        // Load OpenGL functions
        if (!gladLoadGL(glfwGetProcAddress)) {
            throw std::runtime_error("Failed to load OpenGL functions");
        }

        std::cout << "OpenGL Context initialized: "
                  << glGetString(GL_VERSION) << std::endl;
    }

    ~OffscreenContext() {
        if (m_window) {
            m_window->Destroy();
        }
    }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    int m_width;
    int m_height;
    std::unique_ptr<Nova::Window> m_window;
};

/**
 * @brief Detect asset type from JSON
 */
AssetType DetectAssetType(const json& assetData) {
    // Check for type field
    if (assetData.contains("type")) {
        std::string type = assetData["type"].get<std::string>();
        if (type == "unit" || type == "Unit") return AssetType::Unit;
        if (type == "building" || type == "Building") return AssetType::Building;
        if (type == "hero" || type == "Hero") return AssetType::Unit;  // Heroes are units
    }

    // Check for animations
    if (assetData.contains("animations") && !assetData["animations"].empty()) {
        return AssetType::Animated;
    }

    // Check if SDF model has animations
    if (assetData.contains("sdfModel")) {
        const auto& sdfData = assetData["sdfModel"];
        if (sdfData.contains("animations") && !sdfData["animations"].empty()) {
            return AssetType::Animated;
        }
    }

    return AssetType::Static;
}

/**
 * @brief Find idle animation or suitable default
 */
std::string FindIdleAnimation(const json& assetData) {
    // Check top-level animations
    if (assetData.contains("animations")) {
        const auto& anims = assetData["animations"];

        // Priority order: idle, default, first available
        if (anims.contains("idle")) return "idle";
        if (anims.contains("Idle")) return "Idle";
        if (anims.contains("default")) return "default";
        if (anims.contains("Default")) return "Default";

        // Return first animation name
        if (!anims.empty() && anims.is_object()) {
            return anims.begin().key();
        }
    }

    // Check SDF model animations
    if (assetData.contains("sdfModel")) {
        const auto& sdfData = assetData["sdfModel"];
        if (sdfData.contains("animations")) {
            const auto& anims = sdfData["animations"];

            if (anims.contains("idle")) return "idle";
            if (anims.contains("Idle")) return "Idle";
            if (anims.contains("default")) return "default";

            if (!anims.empty() && anims.is_object()) {
                return anims.begin().key();
            }
        }
    }

    return "";  // No animation found
}

/**
 * @brief Load SDF model from JSON asset file
 */
std::unique_ptr<Nova::SDFModel> LoadAssetModel(const std::string& assetPath, const std::string& animationName = "") {
    std::cout << "Loading asset: " << assetPath << std::endl;

    // Read JSON file
    std::ifstream file(assetPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open asset file: " + assetPath);
    }

    json assetData;
    file >> assetData;
    file.close();

    // Verify it has an SDF model
    if (!assetData.contains("sdfModel")) {
        throw std::runtime_error("Asset does not contain sdfModel");
    }

    json sdfData = assetData["sdfModel"];

    // Create model
    std::string modelName = assetData.value("name", "asset_model");
    auto model = std::make_unique<Nova::SDFModel>(modelName);

    // Parse primitives
    if (!sdfData.contains("primitives")) {
        throw std::runtime_error("SDF model has no primitives");
    }

    std::cout << "Found " << sdfData["primitives"].size() << " primitives" << std::endl;

    // Create root primitive (first one becomes root)
    bool isFirst = true;
    Nova::SDFPrimitive* root = nullptr;

    for (const auto& primData : sdfData["primitives"]) {
        // Parse primitive type
        std::string typeStr = primData.value("type", "Sphere");
        Nova::SDFPrimitiveType type = Nova::SDFPrimitiveType::Sphere;

        if (typeStr == "Sphere") type = Nova::SDFPrimitiveType::Sphere;
        else if (typeStr == "Box") type = Nova::SDFPrimitiveType::Box;
        else if (typeStr == "RoundedBox") type = Nova::SDFPrimitiveType::RoundedBox;
        else if (typeStr == "Ellipsoid") type = Nova::SDFPrimitiveType::Ellipsoid;
        else if (typeStr == "Cylinder") type = Nova::SDFPrimitiveType::Cylinder;
        else if (typeStr == "Capsule") type = Nova::SDFPrimitiveType::Capsule;
        else if (typeStr == "Torus") type = Nova::SDFPrimitiveType::Torus;
        else if (typeStr == "Cone") type = Nova::SDFPrimitiveType::Cone;

        // Create primitive
        std::string primName = primData.value("id", "primitive");
        Nova::SDFPrimitive* prim = nullptr;

        if (isFirst) {
            auto rootPrim = std::make_unique<Nova::SDFPrimitive>(primName, type);
            root = rootPrim.get();
            model->SetRoot(std::move(rootPrim));
            prim = root;
            isFirst = false;
        } else {
            prim = model->CreatePrimitive(primName, type, root);
        }

        // Parse transform
        Nova::SDFTransform transform;

        if (primData.contains("transform")) {
            const auto& trans = primData["transform"];

            if (trans.contains("position")) {
                const auto& pos = trans["position"];
                transform.position = glm::vec3(
                    pos[0].get<float>(),
                    pos[1].get<float>(),
                    pos[2].get<float>()
                );
            }

            if (trans.contains("rotation")) {
                const auto& rot = trans["rotation"];
                transform.rotation = glm::quat(
                    rot[3].get<float>(),  // w
                    rot[0].get<float>(),  // x
                    rot[1].get<float>(),  // y
                    rot[2].get<float>()   // z
                );
            }

            if (trans.contains("scale")) {
                const auto& scl = trans["scale"];
                transform.scale = glm::vec3(
                    scl[0].get<float>(),
                    scl[1].get<float>(),
                    scl[2].get<float>()
                );
            }
        }

        prim->SetLocalTransform(transform);

        // Parse parameters
        Nova::SDFParameters params = prim->GetParameters();

        if (primData.contains("params")) {
            const auto& paramsJson = primData["params"];

            if (paramsJson.contains("radius")) {
                params.radius = paramsJson["radius"].get<float>();
                // For cones, the shader expects radius in bottomRadius (parameters2.z)
                if (type == Nova::SDFPrimitiveType::Cone) {
                    params.bottomRadius = paramsJson["radius"].get<float>();
                }
                // For torus, set majorRadius
                if (type == Nova::SDFPrimitiveType::Torus) {
                    params.majorRadius = paramsJson["radius"].get<float>();
                }
            }
            if (paramsJson.contains("size")) {
                const auto& size = paramsJson["size"];
                params.dimensions = glm::vec3(
                    size[0].get<float>(),
                    size[1].get<float>(),
                    size[2].get<float>()
                );
            }
            if (paramsJson.contains("radii")) {
                const auto& radii = paramsJson["radii"];
                params.radii = glm::vec3(
                    radii[0].get<float>(),
                    radii[1].get<float>(),
                    radii[2].get<float>()
                );
            }
            if (paramsJson.contains("height")) {
                params.height = paramsJson["height"].get<float>();
            }
            // Torus tube radius
            if (paramsJson.contains("tubeRadius")) {
                params.minorRadius = paramsJson["tubeRadius"].get<float>();
            }
            // Truncated cone (radius1/radius2) - use larger as bottom radius
            if (paramsJson.contains("radius1") || paramsJson.contains("radius2")) {
                float r1 = paramsJson.value("radius1", 0.1f);
                float r2 = paramsJson.value("radius2", 0.1f);
                params.bottomRadius = std::max(r1, r2);
                params.topRadius = std::min(r1, r2);
                params.radius = params.bottomRadius;  // For general SDF evaluation
            }

            // Onion shell parameters (for clothing layers)
            if (paramsJson.contains("onionThickness")) {
                params.onionThickness = paramsJson["onionThickness"].get<float>();
                params.flags |= 1;  // SDF_FLAG_ONION
            }
            if (paramsJson.contains("shellMinY")) {
                params.shellMinY = paramsJson["shellMinY"].get<float>();
                params.flags |= 2;  // SDF_FLAG_SHELL_BOUNDED
            }
            if (paramsJson.contains("shellMaxY")) {
                params.shellMaxY = paramsJson["shellMaxY"].get<float>();
                params.flags |= 2;  // SDF_FLAG_SHELL_BOUNDED
            }
        }

        prim->SetParameters(params);

        // Parse material
        Nova::SDFMaterial material = prim->GetMaterial();

        if (primData.contains("material")) {
            const auto& mat = primData["material"];

            // Support both "albedo" and "baseColor" for color
            if (mat.contains("baseColor")) {
                const auto& color = mat["baseColor"];
                material.baseColor = glm::vec4(
                    color[0].get<float>(),
                    color[1].get<float>(),
                    color[2].get<float>(),
                    color.size() > 3 ? color[3].get<float>() : 1.0f
                );
            } else if (mat.contains("albedo")) {
                const auto& color = mat["albedo"];
                material.baseColor = glm::vec4(
                    color[0].get<float>(),
                    color[1].get<float>(),
                    color[2].get<float>(),
                    1.0f
                );
            }

            if (mat.contains("metallic")) {
                material.metallic = mat["metallic"].get<float>();
            }

            if (mat.contains("roughness")) {
                material.roughness = mat["roughness"].get<float>();
            }

            if (mat.contains("emissive")) {
                const auto& emissive = mat["emissive"];
                material.emissiveColor = glm::vec3(
                    emissive[0].get<float>(),
                    emissive[1].get<float>(),
                    emissive[2].get<float>()
                );
                material.emissive = mat.value("emissiveStrength", 1.0f);
            }

            // Support emissiveColor as separate key (used by some assets)
            if (mat.contains("emissiveColor")) {
                const auto& emissive = mat["emissiveColor"];
                material.emissiveColor = glm::vec3(
                    emissive[0].get<float>(),
                    emissive[1].get<float>(),
                    emissive[2].get<float>()
                );
                if (mat.contains("emissiveIntensity")) {
                    material.emissive = mat["emissiveIntensity"].get<float>();
                }
            }
        }

        prim->SetMaterial(material);

        // Parse CSG operation
        if (primData.contains("operation")) {
            std::string opStr = primData["operation"].get<std::string>();

            if (opStr == "Union")
                prim->SetCSGOperation(Nova::CSGOperation::Union);
            else if (opStr == "Subtraction")
                prim->SetCSGOperation(Nova::CSGOperation::Subtraction);
            else if (opStr == "Intersection")
                prim->SetCSGOperation(Nova::CSGOperation::Intersection);
            else if (opStr == "SmoothUnion" || opStr == "CubicSmoothUnion" || opStr == "ExponentialSmoothUnion")
                prim->SetCSGOperation(Nova::CSGOperation::SmoothUnion);

            if (primData.contains("smoothness")) {
                Nova::SDFParameters p = prim->GetParameters();
                p.smoothness = primData["smoothness"].get<float>();
                prim->SetParameters(p);
            }
        }
    }

    std::cout << "Model loaded successfully" << std::endl;
    return model;
}

/**
 * @brief Load animation clip from asset JSON
 */
std::shared_ptr<Nova::SDFAnimationClip> LoadAnimation(const std::string& assetPath, const std::string& animationName) {
    std::ifstream file(assetPath);
    if (!file.is_open()) {
        return nullptr;
    }

    json assetData;
    file >> assetData;
    file.close();

    // Look for animations in various locations
    json animData;

    if (assetData.contains("animations") && assetData["animations"].contains(animationName)) {
        animData = assetData["animations"][animationName];
    } else if (assetData.contains("sdfModel") &&
               assetData["sdfModel"].contains("animations") &&
               assetData["sdfModel"]["animations"].contains(animationName)) {
        animData = assetData["sdfModel"]["animations"][animationName];
    } else {
        std::cout << "Animation '" << animationName << "' not found in asset" << std::endl;
        return nullptr;
    }

    auto clip = std::make_shared<Nova::SDFAnimationClip>(animationName);

    // Parse animation properties
    float duration = animData.value("duration", 1.0f);
    clip->SetDuration(duration);
    clip->SetLooping(animData.value("loop", true));
    clip->SetFrameRate(animData.value("fps", 30.0f));

    // Parse keyframes
    if (animData.contains("keyframes")) {
        for (const auto& kfData : animData["keyframes"]) {
            float time = kfData.value("time", 0.0f);
            auto* keyframe = clip->AddKeyframe(time);

            if (kfData.contains("transforms")) {
                for (auto it = kfData["transforms"].begin(); it != kfData["transforms"].end(); ++it) {
                    Nova::SDFTransform transform;

                    if (it.value().contains("position")) {
                        const auto& pos = it.value()["position"];
                        transform.position = glm::vec3(pos[0], pos[1], pos[2]);
                    }

                    if (it.value().contains("rotation")) {
                        const auto& rot = it.value()["rotation"];
                        transform.rotation = glm::quat(rot[3], rot[0], rot[1], rot[2]);
                    }

                    if (it.value().contains("scale")) {
                        const auto& scl = it.value()["scale"];
                        transform.scale = glm::vec3(scl[0], scl[1], scl[2]);
                    }

                    keyframe->transforms[it.key()] = transform;
                }
            }

            keyframe->easing = kfData.value("easing", "linear");
        }
    }

    clip->SortKeyframes();
    std::cout << "Loaded animation '" << animationName << "' with " << clip->GetKeyframeCount()
              << " keyframes, duration " << duration << "s" << std::endl;

    return clip;
}

/**
 * @brief Setup camera for asset rendering
 * Uses IQ-style isometric framing targeting upper body for characters
 */
Nova::Camera CreateAssetCamera(int width, int height, const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    Nova::Camera camera;

    // Calculate model dimensions
    glm::vec3 size = boundsMax - boundsMin;
    float maxDim = glm::max(size.x, glm::max(size.y, size.z));

    // Target point at 65% height for better character framing (upper body/face)
    // This works better for humanoid models than geometric center
    glm::vec3 targetPoint = glm::vec3(
        (boundsMin.x + boundsMax.x) * 0.5f,
        boundsMin.y + size.y * 0.65f,  // 65% up from bottom
        (boundsMin.z + boundsMax.z) * 0.5f
    );

    // Position camera to frame the model with some padding
    float distance = maxDim * 2.0f;  // Slightly further for full character view

    // Camera at 15 degree horizontal angle, 20 degrees above (front-facing hero portrait)
    float angleH = glm::radians(15.0f);
    float angleV = glm::radians(20.0f);

    // Calculate camera position relative to target (spherical coordinates)
    // Camera at NEGATIVE Z to see the front face (model front faces +Z)
    glm::vec3 cameraOffset = glm::vec3(
        distance * cos(angleV) * sin(angleH),  // Small X offset for 3/4 view
        distance * sin(angleV),                 // Y offset for slight downward look
        -distance * cos(angleV) * cos(angleH)  // NEGATIVE Z - camera in FRONT of model
    );
    glm::vec3 cameraPos = targetPoint + cameraOffset;

    // Use LookAt to position camera
    camera.LookAt(cameraPos, targetPoint, glm::vec3(0, 1, 0));
    camera.SetPerspective(35.0f, (float)width / (float)height, 0.1f, 1000.0f);

    return camera;
}

/**
 * @brief View direction names for 6-view validation
 */
const char* VIEW_NAMES[] = {
    "front",   // +Z
    "back",    // -Z
    "left",    // -X
    "right",   // +X
    "top",     // +Y
    "bottom"   // -Y
};

/**
 * @brief Create orthographic camera for 6-view validation
 * @param viewIndex 0=front, 1=back, 2=left, 3=right, 4=top, 5=bottom
 */
Nova::Camera Create6ViewCamera(int viewIndex, int width, int height,
                                const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    Nova::Camera camera;

    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 size = boundsMax - boundsMin;
    float maxDim = glm::max(size.x, glm::max(size.y, size.z)) * 1.2f;

    float distance = maxDim * 2.0f;
    glm::vec3 cameraPos;
    glm::vec3 upVector(0, 1, 0);

    switch (viewIndex) {
        case 0: // Front (+Z)
            cameraPos = center + glm::vec3(0, 0, distance);
            break;
        case 1: // Back (-Z)
            cameraPos = center + glm::vec3(0, 0, -distance);
            break;
        case 2: // Left (-X)
            cameraPos = center + glm::vec3(-distance, 0, 0);
            break;
        case 3: // Right (+X)
            cameraPos = center + glm::vec3(distance, 0, 0);
            break;
        case 4: // Top (+Y)
            cameraPos = center + glm::vec3(0, distance, 0);
            upVector = glm::vec3(0, 0, -1);  // Look down
            break;
        case 5: // Bottom (-Y)
            cameraPos = center + glm::vec3(0, -distance, 0);
            upVector = glm::vec3(0, 0, 1);   // Look up
            break;
        default:
            cameraPos = center + glm::vec3(0, 0, distance);
    }

    camera.LookAt(cameraPos, center, upVector);

    // Use orthographic for cleaner validation views
    float orthoSize = maxDim * 0.6f;
    float aspect = (float)width / (float)height;
    camera.SetOrthographic(-orthoSize * aspect, orthoSize * aspect,
                           -orthoSize, orthoSize, 0.1f, 1000.0f);

    return camera;
}

/**
 * @brief Generate distinct debug colors for primitives (golden ratio hue spacing)
 * Based on Inigo Quilez's technique for visually distinct colors
 */
glm::vec4 GenerateDebugColor(int primitiveIndex, int totalPrimitives) {
    // Golden ratio for maximum color separation
    const float goldenRatio = 0.618033988749895f;
    float hue = fmod(primitiveIndex * goldenRatio, 1.0f);

    // High saturation and value for visibility
    float saturation = 0.85f;
    float value = 0.95f;

    // HSV to RGB conversion
    int hi = (int)(hue * 6.0f);
    float f = hue * 6.0f - hi;
    float p = value * (1.0f - saturation);
    float q = value * (1.0f - f * saturation);
    float t = value * (1.0f - (1.0f - f) * saturation);

    glm::vec3 rgb;
    switch (hi % 6) {
        case 0: rgb = glm::vec3(value, t, p); break;
        case 1: rgb = glm::vec3(q, value, p); break;
        case 2: rgb = glm::vec3(p, value, t); break;
        case 3: rgb = glm::vec3(p, q, value); break;
        case 4: rgb = glm::vec3(t, p, value); break;
        case 5: rgb = glm::vec3(value, p, q); break;
        default: rgb = glm::vec3(1, 1, 1);
    }

    return glm::vec4(rgb, 1.0f);
}

/**
 * @brief Apply debug colors to model primitives for validation
 */
void ApplyDebugColors(Nova::SDFModel& model) {
    auto primitives = model.GetAllPrimitives();
    int total = static_cast<int>(primitives.size());
    int index = 0;

    for (auto* prim : primitives) {
        if (prim && prim->IsVisible()) {
            Nova::SDFMaterial mat = prim->GetMaterial();
            glm::vec4 debugColor = GenerateDebugColor(index, total);

            mat.baseColor = debugColor;
            mat.metallic = 0.0f;      // No reflections for clarity
            mat.roughness = 0.8f;     // Matte for even lighting
            mat.emissive = 0.1f;      // Slight self-illumination for visibility

            prim->SetMaterial(mat);
            index++;
        }
    }

    std::cout << "Applied debug colors to " << index << " primitives" << std::endl;
}

/**
 * @brief Configure AAA quality render settings based on IQ's techniques
 * Soft shadows, AO, proper materials
 */
void ConfigureHighQualitySettings(Nova::SDFRenderer& renderer) {
    auto& settings = renderer.GetSettings();

    // High step count for accurate raymarching
    settings.maxSteps = 256;
    settings.maxDistance = 200.0f;
    settings.hitThreshold = 0.0005f;  // Sub-pixel accuracy

    // Enable all quality features
    settings.enableShadows = true;
    settings.enableAO = true;
    settings.enableReflections = true;

    // Soft shadows (IQ technique: higher k = softer)
    settings.shadowSoftness = 16.0f;
    settings.shadowSteps = 64;

    // Ambient occlusion for depth
    settings.aoSteps = 8;
    settings.aoDistance = 0.5f;
    settings.aoIntensity = 0.6f;

    // Three-point lighting setup
    settings.lightDirection = glm::vec3(0.5f, -0.8f, 0.3f);
    settings.lightColor = glm::vec3(1.0f, 0.98f, 0.95f);  // Warm key light
    settings.lightIntensity = 1.5f;

    // Neutral background for icons
    settings.backgroundColor = glm::vec3(0.12f, 0.12f, 0.14f);
}

/**
 * @brief Render 6-view validation images
 */
int Render6ViewValidation(const RenderConfig& config) {
    std::cout << "\n========== RENDERING 6-VIEW VALIDATION ==========" << std::endl;

    try {
        // Create output directory
        fs::path outPath(config.outputPath);
        fs::path outDir = outPath.parent_path() / (outPath.stem().string() + "_views");
        fs::create_directories(outDir);
        std::cout << "Output directory: " << outDir << std::endl;

        // Create OpenGL context
        std::cout << "[1/6] Creating OpenGL context..." << std::endl;
        OffscreenContext context(config.width, config.height);

        // Load asset model
        std::cout << "[2/6] Loading SDF model..." << std::endl;
        auto model = LoadAssetModel(config.assetPath);

        // Apply debug colors if requested
        if (config.debugColors) {
            std::cout << "[3/6] Applying debug colors..." << std::endl;
            ApplyDebugColors(*model);
        }

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();

        // Create renderer
        std::cout << "[4/6] Initializing SDF renderer..." << std::endl;
        Nova::SDFRenderer renderer;
        if (!renderer.Initialize()) {
            throw std::runtime_error("Failed to initialize SDF renderer");
        }

        // Configure quality
        if (config.highQuality) {
            ConfigureHighQualitySettings(renderer);
        } else {
            auto& settings = renderer.GetSettings();
            settings.maxSteps = 128;
            settings.enableShadows = config.validateShadows || config.highQuality;
            settings.enableAO = true;
            settings.lightDirection = glm::vec3(0.5f, -1.0f, 0.5f);
            settings.lightIntensity = 1.2f;
        }

        // Create framebuffer
        auto framebuffer = std::make_shared<Nova::Framebuffer>();
        if (!framebuffer->Create(config.width, config.height, 1, true)) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, config.width, config.height);

        // Render each view
        std::cout << "[5/6] Rendering 6 views..." << std::endl;
        for (int view = 0; view < 6; view++) {
            std::cout << "  Rendering " << VIEW_NAMES[view] << " view..." << std::endl;

            // Create camera for this view
            Nova::Camera camera = Create6ViewCamera(view, config.width, config.height,
                                                     boundsMin, boundsMax);

            // Render
            framebuffer->Bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderer.Render(*model, camera, glm::mat4(1.0f));

            Nova::Framebuffer::Unbind();

            // Save view
            fs::path viewPath = outDir / (std::string(VIEW_NAMES[view]) + ".png");
            if (!SaveFramebufferToPNG(*framebuffer, viewPath.string())) {
                std::cerr << "Failed to save " << VIEW_NAMES[view] << " view" << std::endl;
            }
        }

        // Also render the standard isometric view
        std::cout << "  Rendering isometric view..." << std::endl;
        Nova::Camera isoCamera = CreateAssetCamera(config.width, config.height, boundsMin, boundsMax);

        framebuffer->Bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.Render(*model, isoCamera, glm::mat4(1.0f));
        Nova::Framebuffer::Unbind();

        fs::path isoPath = outDir / "isometric.png";
        SaveFramebufferToPNG(*framebuffer, isoPath.string());

        // Create composite image (2x3 grid + isometric)
        std::cout << "[6/6] Creating composite validation image..." << std::endl;

        std::cout << "\n✓ SUCCESS! 6-view validation rendered to: " << outDir << std::endl;
        std::cout << "  Views: front, back, left, right, top, bottom, isometric" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}

/**
 * @brief Save framebuffer to PNG with transparency
 */
bool SaveFramebufferToPNG(const Nova::Framebuffer& fb, const std::string& outputPath) {
    int width = fb.GetWidth();
    int height = fb.GetHeight();

    std::cout << "Framebuffer ID: " << fb.GetID() << ", Size: " << width << "x" << height << std::endl;

    // Read pixels from framebuffer (RGBA)
    std::vector<uint8_t> pixels(width * height * 4);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.GetID());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Debug: print corner pixel
    std::cout << "Corner pixel [0]: R=" << (int)pixels[0] << " G=" << (int)pixels[1]
              << " B=" << (int)pixels[2] << " A=" << (int)pixels[3] << std::endl;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Flip vertically (OpenGL origin is bottom-left)
    std::vector<uint8_t> flipped(width * height * 4);
    for (int y = 0; y < height; y++) {
        memcpy(
            flipped.data() + y * width * 4,
            pixels.data() + (height - 1 - y) * width * 4,
            width * 4
        );
    }

    // Write PNG
    int result = stbi_write_png(
        outputPath.c_str(),
        width, height,
        4,  // RGBA
        flipped.data(),
        width * 4  // stride
    );

    return result != 0;
}

/**
 * @brief Render static icon to PNG
 */
int RenderStaticIcon(const RenderConfig& config) {
    std::cout << "\n========== RENDERING STATIC ICON ==========" << std::endl;

    try {
        // Create OpenGL context
        std::cout << "[1/5] Creating OpenGL context..." << std::endl;
        OffscreenContext context(config.width, config.height);

        // Load asset model
        std::cout << "[2/5] Loading SDF model..." << std::endl;
        auto model = LoadAssetModel(config.assetPath);

        // Apply debug colors if requested
        if (config.debugColors) {
            std::cout << "  Applying debug colors..." << std::endl;
            ApplyDebugColors(*model);
        }

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();

        // Create camera
        std::cout << "[3/5] Setting up camera..." << std::endl;
        Nova::Camera camera = CreateAssetCamera(config.width, config.height, boundsMin, boundsMax);

        // Create renderer
        std::cout << "[4/5] Initializing SDF renderer..." << std::endl;
        Nova::SDFRenderer renderer;
        if (!renderer.Initialize()) {
            throw std::runtime_error("Failed to initialize SDF renderer");
        }

        // Configure render settings
        if (config.highQuality) {
            ConfigureHighQualitySettings(renderer);
        } else {
            auto& settings = renderer.GetSettings();
            settings.maxSteps = 256;  // More steps for better quality
            settings.enableShadows = config.validateShadows || true;
            settings.enableAO = true;
            settings.backgroundColor = glm::vec3(0.08f, 0.08f, 0.12f);  // Dark blue-gray for icon background
            settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
            settings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
            settings.lightIntensity = 1.2f;
        }

        // Create framebuffer
        auto framebuffer = std::make_shared<Nova::Framebuffer>();
        if (!framebuffer->Create(config.width, config.height, 1, true)) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Render to framebuffer
        std::cout << "[5/5] Rendering asset..." << std::endl;
        framebuffer->Bind();

        // Set clear color to transparent black
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, config.width, config.height);

        // No transform - let's check orientation of each model
        renderer.Render(*model, camera, glm::mat4(1.0f));

        Nova::Framebuffer::Unbind();

        // Save to PNG
        std::cout << "Saving PNG to: " << config.outputPath << std::endl;

        // Create output directory if needed
        fs::path outPath(config.outputPath);
        fs::create_directories(outPath.parent_path());

        if (!SaveFramebufferToPNG(*framebuffer, config.outputPath)) {
            std::cerr << "Failed to write PNG file" << std::endl;
            return 1;
        }

        std::cout << "\n✓ SUCCESS! Static icon rendered to PNG." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}

/**
 * @brief Render animated asset to video using FFmpeg pipe or image sequence
 */
int RenderAnimatedVideo(const RenderConfig& config, const std::string& animationName) {
    std::cout << "\n========== RENDERING ANIMATED VIDEO ==========" << std::endl;
    std::cout << "Animation: " << animationName << std::endl;
    std::cout << "Duration: " << config.duration << "s @ " << config.fps << " fps" << std::endl;

    int totalFrames = static_cast<int>(config.duration * config.fps);
    std::cout << "Total frames: " << totalFrames << std::endl;

    try {
        // Create OpenGL context
        std::cout << "\n[1/7] Creating OpenGL context..." << std::endl;
        OffscreenContext context(config.width, config.height);

        // Load asset model
        std::cout << "[2/7] Loading SDF model..." << std::endl;
        auto model = LoadAssetModel(config.assetPath);

        // Load animation clip
        std::cout << "[3/7] Loading animation '" << animationName << "'..." << std::endl;
        auto animClip = LoadAnimation(config.assetPath, animationName);

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();

        // Create camera
        std::cout << "[4/7] Setting up camera..." << std::endl;
        Nova::Camera camera = CreateAssetCamera(config.width, config.height, boundsMin, boundsMax);

        // Create renderer
        std::cout << "[5/7] Initializing SDF renderer..." << std::endl;
        Nova::SDFRenderer renderer;
        if (!renderer.Initialize()) {
            throw std::runtime_error("Failed to initialize SDF renderer");
        }

        // Configure render settings
        auto& settings = renderer.GetSettings();
        settings.maxSteps = 128;
        settings.enableShadows = true;
        settings.enableAO = true;
        settings.backgroundColor = glm::vec3(0, 0, 0);
        settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
        settings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        settings.lightIntensity = 1.2f;

        // Create framebuffer
        auto framebuffer = std::make_shared<Nova::Framebuffer>();
        if (!framebuffer->Create(config.width, config.height, 1, true)) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Determine output format
        fs::path outPath(config.outputPath);
        std::string ext = outPath.extension().string();
        bool useFFmpeg = (ext == ".mp4" || ext == ".MP4" || ext == ".avi" || ext == ".AVI" ||
                          ext == ".webm" || ext == ".WEBM" || ext == ".mov" || ext == ".MOV");

        // Pixel buffer for reading framebuffer
        std::vector<uint8_t> pixels(config.width * config.height * 4);

        // FFmpeg pipe (if encoding directly)
        FILE* ffmpegPipe = nullptr;
        fs::path frameDir;

        if (useFFmpeg) {
            // Try to use FFmpeg for direct video encoding
            std::cout << "[6/7] Starting FFmpeg encoder..." << std::endl;

            // Build FFmpeg command
            std::string ffmpegCmd = "ffmpeg -y -f rawvideo -pix_fmt rgba -s " +
                std::to_string(config.width) + "x" + std::to_string(config.height) +
                " -r " + std::to_string(config.fps) +
                " -i - -c:v libx264 -pix_fmt yuv420p -crf 18 -preset fast \"" +
                config.outputPath + "\" 2>/dev/null";

#ifdef _WIN32
            ffmpegPipe = _popen(ffmpegCmd.c_str(), "wb");
#else
            ffmpegPipe = popen(ffmpegCmd.c_str(), "w");
#endif

            if (!ffmpegPipe) {
                std::cout << "  FFmpeg not available, falling back to image sequence" << std::endl;
                useFFmpeg = false;
            }
        }

        if (!useFFmpeg) {
            // Create output directory for frame sequence
            frameDir = outPath.parent_path() / (outPath.stem().string() + "_frames");
            fs::create_directories(frameDir);
            std::cout << "[6/7] Saving frames to: " << frameDir << std::endl;
        }

        std::cout << "[7/7] Rendering " << totalFrames << " frames..." << std::endl;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, config.width, config.height);

        // Render each frame
        for (int frame = 0; frame < totalFrames; frame++) {
            float time = (float)frame / (float)config.fps;
            float progress = (float)frame / (float)totalFrames;

            // Apply animation to model if we have a clip
            if (animClip) {
                // Handle looping: wrap time to animation duration
                float animTime = time;
                if (animClip->IsLooping() && animClip->GetDuration() > 0) {
                    animTime = fmod(time, animClip->GetDuration());
                }
                animClip->ApplyToModel(*model, animTime);
            }

            if (frame % 10 == 0) {
                std::cout << "  Frame " << frame << "/" << totalFrames
                          << " (" << static_cast<int>(progress * 100) << "%)" << std::endl;
            }

            // Render frame
            framebuffer->Bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderer.Render(*model, camera, glm::mat4(1.0f));

            // Read pixels from framebuffer
            glReadPixels(0, 0, config.width, config.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            Nova::Framebuffer::Unbind();

            if (useFFmpeg && ffmpegPipe) {
                // Flip vertically for correct orientation (OpenGL is bottom-up)
                std::vector<uint8_t> flipped(config.width * config.height * 4);
                for (int y = 0; y < config.height; y++) {
                    memcpy(
                        flipped.data() + y * config.width * 4,
                        pixels.data() + (config.height - 1 - y) * config.width * 4,
                        config.width * 4
                    );
                }
                fwrite(flipped.data(), 1, flipped.size(), ffmpegPipe);
            } else {
                // Save frame as PNG
                char frameName[256];
                snprintf(frameName, sizeof(frameName), "frame_%04d.png", frame);
                fs::path framePath = frameDir / frameName;

                if (!SaveFramebufferToPNG(*framebuffer, framePath.string())) {
                    std::cerr << "Failed to save frame " << frame << std::endl;
                    return 1;
                }
            }
        }

        // Close FFmpeg pipe
        if (ffmpegPipe) {
#ifdef _WIN32
            _pclose(ffmpegPipe);
#else
            pclose(ffmpegPipe);
#endif
            std::cout << "\n✓ SUCCESS! Video saved to: " << config.outputPath << std::endl;
        } else {
            std::cout << "\nFrames saved to: " << frameDir << std::endl;

            // Provide ffmpeg command for video conversion
            std::cout << "\nTo convert to MP4, run:" << std::endl;
            std::cout << "  ffmpeg -framerate " << config.fps << " -i \""
                      << frameDir.string() << "/frame_%04d.png\" "
                      << "-c:v libx264 -pix_fmt yuv420p -crf 23 \""
                      << config.outputPath << "\"" << std::endl;

            std::cout << "\nTo convert to GIF, run:" << std::endl;
            std::cout << "  ffmpeg -framerate " << config.fps << " -i \""
                      << frameDir.string() << "/frame_%04d.png\" "
                      << "-vf \"scale=" << config.width << ":" << config.height << ":flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" "
                      << "\"" << outPath.stem().string() << ".gif\"" << std::endl;

            std::cout << "\n✓ SUCCESS! Animation frames rendered." << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}

/**
 * @brief Main rendering dispatcher
 */
int RenderAssetMedia(const RenderConfig& config) {
    std::cout << "========================================" << std::endl;
    std::cout << "Asset Media Renderer (GI-Enhanced)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Input:  " << config.assetPath << std::endl;
    std::cout << "Output: " << config.outputPath << std::endl;
    std::cout << "Size:   " << config.width << "x" << config.height << std::endl;
    std::cout << "========================================" << std::endl;

    // Read asset JSON to detect type
    std::ifstream file(config.assetPath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open asset file: " << config.assetPath << std::endl;
        return 1;
    }

    json assetData;
    file >> assetData;
    file.close();

    // Detect asset type
    AssetType assetType = DetectAssetType(assetData);
    std::string animationName;

    if (assetType == AssetType::Unit || assetType == AssetType::Building) {
        animationName = FindIdleAnimation(assetData);
        if (animationName.empty()) {
            std::cout << "No idle animation found, rendering as static" << std::endl;
            assetType = AssetType::Static;
        } else {
            std::cout << "Asset type: " << (assetType == AssetType::Unit ? "Unit" : "Building") << std::endl;
            std::cout << "Using animation: " << animationName << std::endl;
            assetType = AssetType::Animated;
        }
    }

    // Determine output format from extension
    fs::path outPath(config.outputPath);
    std::string ext = outPath.extension().string();
    bool isPNG = (ext == ".png" || ext == ".PNG");
    bool isMP4 = (ext == ".mp4" || ext == ".MP4");
    bool isGIF = (ext == ".gif" || ext == ".GIF");

    // Check for validation/debug mode first
    if (config.render6Views) {
        return Render6ViewValidation(config);
    }

    // Render based on type and format
    if (config.forceStatic || assetType == AssetType::Static || isPNG) {
        return RenderStaticIcon(config);
    } else if (config.forceAnimation || assetType == AssetType::Animated || isMP4 || isGIF) {
        if (animationName.empty()) {
            animationName = config.animationName;
        }
        return RenderAnimatedVideo(config, animationName);
    } else {
        std::cerr << "ERROR: Cannot determine render mode. Use .png for static or .mp4/.gif for animated." << std::endl;
        return 1;
    }
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "ERROR: Failed to initialize GLFW" << std::endl;
        return 1;
    }

    if (argc < 3) {
        std::cout << "Usage: asset_media_renderer <asset.json> <output.png|output.mp4> [options]" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --width <pixels>      Output width (default: 512)" << std::endl;
        std::cout << "  --height <pixels>     Output height (default: 512)" << std::endl;
        std::cout << "  --fps <number>        Frames per second for video (default: 30)" << std::endl;
        std::cout << "  --duration <seconds>  Video duration (default: 3.0)" << std::endl;
        std::cout << "  --animation <name>    Animation to use (default: idle)" << std::endl;
        std::cout << "  --static              Force static rendering even for animated assets" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Validation/Debug Options:" << std::endl;
        std::cout << "  --6view               Render 6 orthographic views (front/back/left/right/top/bottom)" << std::endl;
        std::cout << "  --debug-colors        Apply unique colors to each primitive for debugging" << std::endl;
        std::cout << "  --shadows             Force shadow validation" << std::endl;
        std::cout << "  --high-quality        Use AAA quality settings (slower)" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  # Render static icon" << std::endl;
        std::cout << "  asset_media_renderer hero.json hero_icon.png --width 512 --height 512" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "  # Render animated preview (creates frame sequence)" << std::endl;
        std::cout << "  asset_media_renderer unit.json unit_anim.mp4 --fps 30 --duration 3.0" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "  # Render 6-view validation with debug colors" << std::endl;
        std::cout << "  asset_media_renderer hero.json hero_debug.png --6view --debug-colors" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "  # High quality render with shadows" << std::endl;
        std::cout << "  asset_media_renderer hero.json hero_hq.png --high-quality --shadows" << std::endl;
        return 1;
    }

    RenderConfig config;
    config.assetPath = argv[1];
    config.outputPath = argv[2];

    // Parse optional arguments
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--width" && i + 1 < argc) {
            config.width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::atoi(argv[++i]);
        } else if (arg == "--fps" && i + 1 < argc) {
            config.fps = std::atoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            config.duration = std::atof(argv[++i]);
        } else if (arg == "--animation" && i + 1 < argc) {
            config.animationName = argv[++i];
        } else if (arg == "--static") {
            config.forceStatic = true;
        } else if (arg == "--animated") {
            config.forceAnimation = true;
        } else if (arg == "--6view" || arg == "--6-view" || arg == "--validation") {
            config.render6Views = true;
        } else if (arg == "--debug-colors" || arg == "--debug") {
            config.debugColors = true;
        } else if (arg == "--shadows") {
            config.validateShadows = true;
        } else if (arg == "--high-quality" || arg == "--hq") {
            config.highQuality = true;
        } else if (arg == "--gi") {
            config.validateGI = true;
        }
    }

    // Validate dimensions
    if (config.width <= 0 || config.width > 4096 || config.height <= 0 || config.height > 4096) {
        std::cerr << "Invalid dimensions. Must be between 1 and 4096." << std::endl;
        return 1;
    }

    // Validate FPS and duration
    if (config.fps <= 0 || config.fps > 120) {
        std::cerr << "Invalid FPS. Must be between 1 and 120." << std::endl;
        return 1;
    }

    if (config.duration <= 0.0f || config.duration > 60.0f) {
        std::cerr << "Invalid duration. Must be between 0 and 60 seconds." << std::endl;
        return 1;
    }

    return RenderAssetMedia(config);
}
