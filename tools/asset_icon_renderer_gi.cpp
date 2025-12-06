/**
 * @file asset_icon_renderer.cpp
 * @brief Standalone tool to render SDF asset icons to PNG
 *
 * Usage: asset_icon_renderer <asset.json> <output.png> [width] [height]
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <cstdlib>

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
#include "../engine/scene/Camera.hpp"
#include "../engine/graphics/Shader.hpp"
#include "../engine/core/Window.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * @brief Initialize minimal OpenGL context
 */
class OffscreenContext {
public:
    OffscreenContext(int width, int height)
        : m_width(width), m_height(height) {

        // Create hidden window for OpenGL context
        Nova::Window::CreateParams params;
        params.title = "Asset Icon Renderer";
        params.width = width;
        params.height = height;
        params.vsync = false;
        params.visible = true;

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
 * @brief Load SDF model from JSON asset file
 */
std::unique_ptr<Nova::SDFModel> LoadAssetModel(const std::string& assetPath) {
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

    std::cout << "Camera positioned at: ("
              << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
    std::cout << "Looking at: ("
              << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;

    return camera;
}

/**
 * @brief Save framebuffer to PNG with transparency
 */
bool SaveFramebufferToPNG(const Nova::Framebuffer& fb, const std::string& outputPath) {
    int width = fb.GetWidth();
    int height = fb.GetHeight();

    std::cout << "Reading framebuffer (" << width << "x" << height << ")..." << std::endl;

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

    std::cout << "Writing PNG to: " << outputPath << std::endl;

    // Write PNG
    int result = stbi_write_png(
        outputPath.c_str(),
        width, height,
        4,  // RGBA
        flipped.data(),
        width * 4  // stride
    );

    if (result == 0) {
        std::cerr << "Failed to write PNG file" << std::endl;
        return false;
    }

    std::cout << "PNG saved successfully" << std::endl;
    return true;
}

/**
 * @brief Main rendering function
 */
int RenderAssetIcon(const std::string& assetPath, const std::string& outputPath,
                     int width = 512, int height = 512) {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Asset Icon Renderer" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Input:  " << assetPath << std::endl;
        std::cout << "Output: " << outputPath << std::endl;
        std::cout << "Size:   " << width << "x" << height << std::endl;
        std::cout << "========================================" << std::endl;

        // Create OpenGL context
        std::cout << "\n[1/5] Creating OpenGL context..." << std::endl;
        OffscreenContext context(width, height);

        // Load asset model
        std::cout << "\n[2/5] Loading SDF model..." << std::endl;
        auto model = LoadAssetModel(assetPath);

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();
        std::cout << "Model bounds: ["
                  << boundsMin.x << ", " << boundsMin.y << ", " << boundsMin.z << "] to ["
                  << boundsMax.x << ", " << boundsMax.y << ", " << boundsMax.z << "]" << std::endl;

        // Create camera
        std::cout << "\n[3/5] Setting up camera..." << std::endl;
        Nova::Camera camera = CreateAssetCamera(width, height, boundsMin, boundsMax);

        // Create renderer
        std::cout << "\n[4/5] Initializing SDF renderer..." << std::endl;
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
        if (!framebuffer->Create(width, height, 1, true)) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Render to framebuffer
        std::cout << "\n[5/5] Rendering asset..." << std::endl;
        framebuffer->Bind();
        framebuffer->Clear(glm::vec4(0, 0, 0, 0));  // Clear to transparent

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, width, height);

        renderer.RenderToTexture(*model, camera, framebuffer);

        Nova::Framebuffer::Unbind();

        // Save to PNG
        std::cout << "\nSaving PNG..." << std::endl;

        // Create output directory if needed
        fs::path outPath(outputPath);
        fs::create_directories(outPath.parent_path());

        if (!SaveFramebufferToPNG(*framebuffer, outputPath)) {
            return 1;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "SUCCESS! Asset icon rendered." << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
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
        std::cout << "Usage: asset_icon_renderer <asset.json> <output.png> [width] [height]" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  asset_icon_renderer alien_commander.json output.png" << std::endl;
        std::cout << "  asset_icon_renderer alien_commander.json output.png 1024 1024" << std::endl;
        return 1;
    }

    std::string assetPath = argv[1];
    std::string outputPath = argv[2];
    int width = 512;
    int height = 512;

    if (argc >= 4) {
        width = std::atoi(argv[3]);
    }
    if (argc >= 5) {
        height = std::atoi(argv[4]);
    }

    // Validate dimensions
    if (width <= 0 || width > 4096 || height <= 0 || height > 4096) {
        std::cerr << "Invalid dimensions. Must be between 1 and 4096." << std::endl;
        return 1;
    }

    return RenderAssetIcon(assetPath, outputPath, width, height);
}
