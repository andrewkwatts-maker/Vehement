#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>

#include <glm/glm.hpp>

namespace Vehement {

/**
 * @brief Building types for procedural model generation
 */
enum class BuildingType : uint8_t {
    Shelter,        ///< Small cube house (1x1 hex)
    House,          ///< Larger house (1x1 hex)
    Barracks,       ///< Long building (2x1 hex)
    Workshop,       ///< Industrial building (1x1 hex)
    Farm,           ///< Flat with fences (2x2 hex)
    Watchtower,     ///< Tall narrow tower (1x1 hex)
    WallStraight,   ///< Hex edge wall
    WallCorner,     ///< Hex corner wall
    Gate,           ///< Wall with opening
    Fortress        ///< Large castle (3x3 hex)
};

/**
 * @brief Tree types for procedural model generation
 */
enum class TreeType : uint8_t {
    Pine,           ///< Conical pine tree
    Oak             ///< Round oak tree
};

/**
 * @brief Resource types for procedural model generation
 */
enum class ResourceType : uint8_t {
    RockSmall,      ///< Small boulder
    RockLarge,      ///< Large rock formation
    Bush,           ///< Shrub
    Crate           ///< Supply crate
};

/**
 * @brief Unit types for procedural model generation
 */
enum class UnitType : uint8_t {
    Hero,           ///< Humanoid hero placeholder
    Worker,         ///< Smaller humanoid worker
    Zombie,         ///< Shambling zombie humanoid
    Guard           ///< Armed guard humanoid
};

/**
 * @brief Hex tile types for procedural model generation
 */
enum class TileType : uint8_t {
    Grass,          ///< Grass terrain
    Dirt,           ///< Dirt terrain
    Stone,          ///< Stone terrain
    Water,          ///< Water terrain
    Road            ///< Road terrain
};

/**
 * @brief Texture generation types
 */
enum class TextureType : uint8_t {
    Noise,          ///< Perlin/simplex noise
    Checker,        ///< Checkerboard pattern
    Brick,          ///< Brick wall pattern
    Wood,           ///< Wood grain pattern
    Water,          ///< Animated water pattern
    Metal,          ///< Metal sheet pattern
    Thatch,         ///< Straw/thatch pattern
    Road,           ///< Road with markings
    Icon            ///< UI icon
};

/**
 * @brief Simple vertex structure for OBJ generation
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

/**
 * @brief Simple mesh data for OBJ generation
 */
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string materialName;
};

/**
 * @brief RGBA pixel structure
 */
struct Pixel {
    uint8_t r, g, b, a;

    Pixel() : r(0), g(0), b(0), a(255) {}
    Pixel(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    static Pixel FromVec3(const glm::vec3& color) {
        return Pixel(
            static_cast<uint8_t>(glm::clamp(color.r * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(glm::clamp(color.g * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(glm::clamp(color.b * 255.0f, 0.0f, 255.0f))
        );
    }

    static Pixel FromVec4(const glm::vec4& color) {
        return Pixel(
            static_cast<uint8_t>(glm::clamp(color.r * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(glm::clamp(color.g * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(glm::clamp(color.b * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(glm::clamp(color.a * 255.0f, 0.0f, 255.0f))
        );
    }
};

/**
 * @brief Simple image buffer for texture generation
 */
class ImageBuffer {
public:
    ImageBuffer(int width, int height);

    void SetPixel(int x, int y, const Pixel& pixel);
    Pixel GetPixel(int x, int y) const;
    void Fill(const Pixel& pixel);
    void Clear();

    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }
    [[nodiscard]] const std::vector<Pixel>& GetData() const { return m_data; }
    [[nodiscard]] std::vector<uint8_t> GetRawData() const;

    // Save to PNG file
    bool SavePNG(const std::string& path) const;

private:
    int m_width;
    int m_height;
    std::vector<Pixel> m_data;
};

/**
 * @brief Procedural placeholder asset generator
 *
 * Generates placeholder 3D models (.obj) and textures (.png) at runtime
 * if they don't exist. All assets are procedurally generated so no
 * external files are needed initially.
 *
 * Usage:
 * @code
 * // Generate all placeholders at startup
 * PlaceholderGenerator::GenerateAllPlaceholders();
 *
 * // Or generate specific assets
 * PlaceholderGenerator::GenerateBuildingModel("shelter.obj", BuildingType::Shelter);
 * PlaceholderGenerator::GenerateNoiseTexture("grass.png", {0.3f, 0.6f, 0.2f}, 256);
 * @endcode
 */
class PlaceholderGenerator {
public:
    // =========================================================================
    // Main Generation Entry Point
    // =========================================================================

    /**
     * @brief Generate all placeholder assets if not present
     * @param basePath Base path for assets directory
     * @param forceRegenerate If true, regenerate even if files exist
     */
    static void GenerateAllPlaceholders(
        const std::string& basePath = "game/assets",
        bool forceRegenerate = false);

    /**
     * @brief Check if all placeholder assets exist
     * @param basePath Base path for assets directory
     * @return true if all assets exist
     */
    static bool AllPlaceholdersExist(const std::string& basePath = "game/assets");

    // =========================================================================
    // Texture Generation
    // =========================================================================

    /**
     * @brief Generate a noise texture (Perlin-like)
     * @param path Output file path
     * @param baseColor Base color for the texture
     * @param size Texture size (width = height)
     * @param variation Color variation amount (0-1)
     * @param scale Noise scale
     */
    static void GenerateNoiseTexture(
        const std::string& path,
        const glm::vec3& baseColor,
        int size = 256,
        float variation = 0.2f,
        float scale = 8.0f);

    /**
     * @brief Generate a normal map (flat or with slight variation)
     * @param path Output file path
     * @param size Texture size
     * @param bumpiness Amount of surface variation (0-1)
     */
    static void GenerateNormalMap(
        const std::string& path,
        int size = 256,
        float bumpiness = 0.0f);

    /**
     * @brief Generate a checker/grid texture
     * @param path Output file path
     * @param color1 First checker color
     * @param color2 Second checker color
     * @param size Texture size
     * @param checkerSize Size of each checker square
     */
    static void GenerateCheckerTexture(
        const std::string& path,
        const glm::vec3& color1,
        const glm::vec3& color2,
        int size = 256,
        int checkerSize = 32);

    /**
     * @brief Generate a brick pattern texture
     * @param path Output file path
     * @param brickColor Brick color
     * @param mortarColor Mortar/grout color
     * @param size Texture size
     * @param brickWidth Brick width in pixels
     * @param brickHeight Brick height in pixels
     * @param mortarWidth Mortar width in pixels
     */
    static void GenerateBrickTexture(
        const std::string& path,
        const glm::vec3& brickColor,
        const glm::vec3& mortarColor,
        int size = 256,
        int brickWidth = 64,
        int brickHeight = 32,
        int mortarWidth = 4);

    /**
     * @brief Generate a wood grain texture
     * @param path Output file path
     * @param baseColor Base wood color
     * @param size Texture size
     * @param grainDensity Density of wood grain lines
     */
    static void GenerateWoodTexture(
        const std::string& path,
        const glm::vec3& baseColor,
        int size = 256,
        float grainDensity = 20.0f);

    /**
     * @brief Generate a metal/steel texture
     * @param path Output file path
     * @param baseColor Base metal color
     * @param size Texture size
     */
    static void GenerateMetalTexture(
        const std::string& path,
        const glm::vec3& baseColor,
        int size = 256);

    /**
     * @brief Generate a thatch/straw texture
     * @param path Output file path
     * @param baseColor Base straw color
     * @param size Texture size
     */
    static void GenerateThatchTexture(
        const std::string& path,
        const glm::vec3& baseColor,
        int size = 256);

    /**
     * @brief Generate a water texture
     * @param path Output file path
     * @param baseColor Base water color
     * @param size Texture size
     * @param waveIntensity Wave pattern intensity
     */
    static void GenerateWaterTexture(
        const std::string& path,
        const glm::vec3& baseColor,
        int size = 256,
        float waveIntensity = 0.3f);

    /**
     * @brief Generate a road texture with markings
     * @param path Output file path
     * @param roadColor Road surface color
     * @param markingColor Road marking color
     * @param size Texture size
     */
    static void GenerateRoadTexture(
        const std::string& path,
        const glm::vec3& roadColor,
        const glm::vec3& markingColor,
        int size = 256);

    /**
     * @brief Generate a simple UI icon
     * @param path Output file path
     * @param iconType Type of icon to generate
     * @param primaryColor Primary icon color
     * @param size Icon size (square)
     */
    static void GenerateIcon(
        const std::string& path,
        const std::string& iconType,
        const glm::vec3& primaryColor,
        int size = 64);

    /**
     * @brief Generate a health/mana bar texture
     * @param path Output file path
     * @param barColor Bar fill color
     * @param backgroundColor Background color
     * @param width Bar width
     * @param height Bar height
     */
    static void GenerateBarTexture(
        const std::string& path,
        const glm::vec3& barColor,
        const glm::vec3& backgroundColor,
        int width = 256,
        int height = 32);

    // =========================================================================
    // Model Generation
    // =========================================================================

    /**
     * @brief Generate a building model
     * @param path Output file path
     * @param type Building type to generate
     */
    static void GenerateBuildingModel(
        const std::string& path,
        BuildingType type);

    /**
     * @brief Generate a tree model
     * @param path Output file path
     * @param type Tree type to generate
     */
    static void GenerateTreeModel(
        const std::string& path,
        TreeType type);

    /**
     * @brief Generate a resource model (rocks, crates, etc.)
     * @param path Output file path
     * @param type Resource type to generate
     */
    static void GenerateResourceModel(
        const std::string& path,
        ResourceType type);

    /**
     * @brief Generate a unit/character model
     * @param path Output file path
     * @param type Unit type to generate
     */
    static void GenerateUnitModel(
        const std::string& path,
        UnitType type);

    /**
     * @brief Generate a hex tile model
     * @param path Output file path
     * @param type Tile type to generate
     */
    static void GenerateHexTileModel(
        const std::string& path,
        TileType type);

    // =========================================================================
    // Primitive Generation Helpers
    // =========================================================================

    /**
     * @brief Generate a box mesh
     */
    static MeshData GenerateBox(
        const glm::vec3& size,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Generate a cylinder mesh
     */
    static MeshData GenerateCylinder(
        float radius,
        float height,
        int segments = 16,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Generate a cone mesh
     */
    static MeshData GenerateCone(
        float radius,
        float height,
        int segments = 16,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Generate a sphere mesh
     */
    static MeshData GenerateSphere(
        float radius,
        int segments = 16,
        int rings = 8,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Generate a hexagon prism mesh
     */
    static MeshData GenerateHexPrism(
        float radius,
        float height,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Generate a pyramid mesh
     */
    static MeshData GeneratePyramid(
        float baseSize,
        float height,
        int sides = 4,
        const glm::vec3& offset = glm::vec3(0.0f));

    /**
     * @brief Combine multiple meshes into one
     */
    static MeshData CombineMeshes(const std::vector<MeshData>& meshes);

    /**
     * @brief Write mesh data to OBJ file
     */
    static bool WriteMeshToOBJ(
        const std::string& path,
        const MeshData& mesh,
        const std::string& materialName = "default");

private:
    // Noise generation helpers
    static float PerlinNoise2D(float x, float y);
    static float FractalNoise2D(float x, float y, int octaves, float persistence);
    static float Lerp(float a, float b, float t);
    static float Fade(float t);
    static float Grad(int hash, float x, float y);

    // Random number generation (deterministic for reproducibility)
    static uint32_t Hash(uint32_t x);
    static float RandomFloat(int x, int y);

    // Texture generation helpers
    static void ApplyNoiseToImage(ImageBuffer& image, const glm::vec3& baseColor, float variation, float scale);
    static void DrawLine(ImageBuffer& image, int x0, int y0, int x1, int y1, const Pixel& color);
    static void DrawCircle(ImageBuffer& image, int cx, int cy, int radius, const Pixel& color, bool filled = true);
    static void DrawRect(ImageBuffer& image, int x, int y, int w, int h, const Pixel& color, bool filled = true);

    // Model generation helpers
    static void TransformMesh(MeshData& mesh, const glm::mat4& transform);
    static void ScaleMesh(MeshData& mesh, const glm::vec3& scale);
    static void TranslateMesh(MeshData& mesh, const glm::vec3& offset);

    // Permutation table for Perlin noise
    static const std::array<int, 512> s_permutation;
};

} // namespace Vehement
