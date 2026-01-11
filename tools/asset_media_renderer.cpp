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

// STB Image Write for PNG output
#define STB_IMAGE_WRITE_IMPLEMENTATION
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
        }

        prim->SetParameters(params);

        // Parse material
        Nova::SDFMaterial material = prim->GetMaterial();

        if (primData.contains("material")) {
            const auto& mat = primData["material"];

            if (mat.contains("albedo")) {
                const auto& albedo = mat["albedo"];
                material.baseColor = glm::vec4(
                    albedo[0].get<float>(),
                    albedo[1].get<float>(),
                    albedo[2].get<float>(),
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

    // TODO: Load animation if animationName is specified
    // This would require SDFAnimation support in the model
    if (!animationName.empty()) {
        std::cout << "Note: Animation '" << animationName << "' requested but not yet implemented in renderer" << std::endl;
    }

    std::cout << "Model loaded successfully" << std::endl;
    return model;
}

/**
 * @brief Setup camera for asset rendering
 */
Nova::Camera CreateAssetCamera(int width, int height, const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    Nova::Camera camera;

    // Calculate model center and size
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 size = boundsMax - boundsMin;
    float maxDim = glm::max(size.x, glm::max(size.y, size.z));

    // Position camera to frame the model
    float distance = maxDim * 2.5f;  // Distance multiplier for good framing

    // Camera at 45 degree angle, slightly above
    float angleH = glm::radians(45.0f);
    float angleV = glm::radians(15.0f);

    glm::vec3 cameraPos = center + glm::vec3(
        distance * cos(angleV) * sin(angleH),
        distance * sin(angleV) + center.y,
        distance * cos(angleV) * cos(angleH)
    );

    // Use LookAt to position camera
    camera.LookAt(cameraPos, center, glm::vec3(0, 1, 0));
    camera.SetPerspective(35.0f, (float)width / (float)height, 0.1f, 1000.0f);

    return camera;
}

/**
 * @brief Save framebuffer to PNG with transparency
 */
bool SaveFramebufferToPNG(const Nova::Framebuffer& fb, const std::string& outputPath) {
    int width = fb.GetWidth();
    int height = fb.GetHeight();

    // Read pixels from framebuffer (RGBA)
    std::vector<uint8_t> pixels(width * height * 4);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.GetID());
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
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
        auto& settings = renderer.GetSettings();
        settings.maxSteps = 128;
        settings.enableShadows = true;
        settings.enableAO = true;
        settings.backgroundColor = glm::vec3(0, 0, 0);  // Transparent background
        settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
        settings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        settings.lightIntensity = 1.2f;

        // Create framebuffer
        auto framebuffer = std::make_shared<Nova::Framebuffer>();
        if (!framebuffer->Create(config.width, config.height, 1, true)) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Render to framebuffer
        std::cout << "[5/5] Rendering asset..." << std::endl;
        framebuffer->Bind();
        framebuffer->Clear(glm::vec4(0, 0, 0, 0));  // Clear to transparent

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, config.width, config.height);

        renderer.RenderToTexture(*model, camera, framebuffer);

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
 * @brief Render animated asset to video (MP4) or image sequence
 *
 * Note: For MP4 encoding, we'd need ffmpeg or similar. For now, we render
 * an image sequence that can be converted to video externally.
 */
int RenderAnimatedVideo(const RenderConfig& config, const std::string& animationName) {
    std::cout << "\n========== RENDERING ANIMATED VIDEO ==========" << std::endl;
    std::cout << "Animation: " << animationName << std::endl;
    std::cout << "Duration: " << config.duration << "s @ " << config.fps << " fps" << std::endl;

    int totalFrames = static_cast<int>(config.duration * config.fps);
    std::cout << "Total frames: " << totalFrames << std::endl;

    try {
        // Create OpenGL context
        std::cout << "\n[1/6] Creating OpenGL context..." << std::endl;
        OffscreenContext context(config.width, config.height);

        // Load asset model with animation
        std::cout << "[2/6] Loading SDF model with animation '" << animationName << "'..." << std::endl;
        auto model = LoadAssetModel(config.assetPath, animationName);

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();

        // Create camera
        std::cout << "[3/6] Setting up camera..." << std::endl;
        Nova::Camera camera = CreateAssetCamera(config.width, config.height, boundsMin, boundsMax);

        // Create renderer
        std::cout << "[4/6] Initializing SDF renderer..." << std::endl;
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

        // Create output directory for frame sequence
        fs::path outPath(config.outputPath);
        fs::path frameDir = outPath.parent_path() / (outPath.stem().string() + "_frames");
        fs::create_directories(frameDir);

        std::cout << "[5/6] Rendering " << totalFrames << " frames..." << std::endl;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, config.width, config.height);

        // Render each frame
        for (int frame = 0; frame < totalFrames; frame++) {
            float time = (float)frame / (float)config.fps;
            float progress = (float)frame / (float)totalFrames;

            // TODO: Update animation state based on time
            // This would require SDFAnimation::UpdateAnimation(time)
            // For now, we just render the static pose

            if (frame % 10 == 0) {
                std::cout << "  Frame " << frame << "/" << totalFrames
                          << " (" << static_cast<int>(progress * 100) << "%)" << std::endl;
            }

            // Render frame
            framebuffer->Bind();
            framebuffer->Clear(glm::vec4(0, 0, 0, 0));

            renderer.RenderToTexture(*model, camera, framebuffer);

            Nova::Framebuffer::Unbind();

            // Save frame
            char frameName[256];
            snprintf(frameName, sizeof(frameName), "frame_%04d.png", frame);
            fs::path framePath = frameDir / frameName;

            if (!SaveFramebufferToPNG(*framebuffer, framePath.string())) {
                std::cerr << "Failed to save frame " << frame << std::endl;
                return 1;
            }
        }

        std::cout << "\n[6/6] All frames rendered successfully!" << std::endl;
        std::cout << "Frames saved to: " << frameDir << std::endl;

        // Provide ffmpeg command for MP4 conversion
        std::cout << "\nTo convert to MP4, run:" << std::endl;
        std::cout << "  ffmpeg -framerate " << config.fps << " -i \""
                  << frameDir.string() << "/frame_%04d.png\" "
                  << "-c:v libx264 -pix_fmt yuv420p -crf 23 \""
                  << config.outputPath << "\"" << std::endl;

        std::cout << "\nTo convert to GIF, run:" << std::endl;
        std::cout << "  ffmpeg -framerate " << config.fps << " -i \""
                  << frameDir.string() << "/frame_%04d.png\" "
                  << "-vf \"scale=" << config.width << ":" << config.height << ":flags=lanczos,palettegen\" "
                  << "-y palette.png && "
                  << "ffmpeg -framerate " << config.fps << " -i \""
                  << frameDir.string() << "/frame_%04d.png\" "
                  << "-i palette.png -lavfi \"scale=" << config.width << ":" << config.height << ":flags=lanczos[x];[x][1:v]paletteuse\" "
                  << "\"" << outPath.replace_extension(".gif").string() << "\"" << std::endl;

        std::cout << "\n✓ SUCCESS! Animation frames rendered." << std::endl;
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
        std::cout << "Examples:" << std::endl;
        std::cout << "  # Render static icon" << std::endl;
        std::cout << "  asset_media_renderer hero.json hero_icon.png --width 512 --height 512" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "  # Render animated preview (creates frame sequence)" << std::endl;
        std::cout << "  asset_media_renderer unit.json unit_anim.mp4 --fps 30 --duration 3.0" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "  # Use specific animation" << std::endl;
        std::cout << "  asset_media_renderer unit.json unit_attack.gif --animation attack" << std::endl;
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
