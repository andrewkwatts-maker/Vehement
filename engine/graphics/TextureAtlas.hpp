#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Texture;

/**
 * @brief Rectangle for atlas packing
 */
struct AtlasRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    bool Intersects(const AtlasRect& other) const {
        return !(x + width <= other.x || other.x + other.width <= x ||
                 y + height <= other.y || other.y + other.height <= y);
    }
};

/**
 * @brief UV coordinates for an atlas entry
 */
struct AtlasUV {
    glm::vec2 min{0.0f};  // Bottom-left UV
    glm::vec2 max{1.0f};  // Top-right UV
    int layer = 0;         // For array textures

    glm::vec2 GetSize() const { return max - min; }
    glm::vec2 GetCenter() const { return (min + max) * 0.5f; }

    // Transform local UVs (0-1) to atlas UVs
    glm::vec2 Transform(const glm::vec2& localUV) const {
        return min + localUV * (max - min);
    }
};

/**
 * @brief Entry in a texture atlas
 */
struct AtlasEntry {
    std::string name;
    uint32_t textureID = 0;      // Original texture ID
    AtlasRect rect;
    AtlasUV uv;
    int atlasIndex = 0;           // Which atlas this belongs to

    // Padding added around texture
    int padding = 1;

    // Original texture dimensions
    int originalWidth = 0;
    int originalHeight = 0;
};

/**
 * @brief Configuration for texture atlas
 */
struct TextureAtlasConfig {
    int maxSize = 4096;           // Maximum atlas texture size
    int padding = 1;              // Padding between textures
    bool generateMipmaps = true;
    bool allowResize = true;      // Allow atlas to grow
    bool powerOfTwo = true;       // Force power-of-two dimensions

    // Compression settings
    bool useCompression = false;
    int compressionQuality = 75;

    // Array texture settings
    bool useArrayTexture = false;
    int maxLayers = 16;
};

/**
 * @brief Node for binary tree packing algorithm
 */
struct PackNode {
    AtlasRect rect;
    int left = -1;    // Index of left child (-1 = none)
    int right = -1;   // Index of right child
    bool used = false;
    int entryIndex = -1;
};

/**
 * @brief Single atlas texture
 */
struct Atlas {
    uint32_t textureID = 0;
    int width = 0;
    int height = 0;
    int usedArea = 0;
    std::vector<PackNode> nodes;
    std::vector<int> entryIndices;

    float GetUtilization() const {
        return static_cast<float>(usedArea) / (width * height);
    }
};

/**
 * @brief Runtime texture atlas system
 *
 * Packs multiple textures into larger atlas textures to reduce
 * texture binds and improve batching efficiency.
 */
class TextureAtlas {
public:
    TextureAtlas();
    ~TextureAtlas();

    // Non-copyable
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    /**
     * @brief Initialize the atlas system
     */
    bool Initialize(const TextureAtlasConfig& config = TextureAtlasConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Add a texture to the atlas
     * @param name Unique identifier for this texture
     * @param texture The texture to add
     * @return true if added successfully
     */
    bool AddTexture(const std::string& name, const std::shared_ptr<Texture>& texture);

    /**
     * @brief Add texture from raw pixel data
     * @param name Unique identifier
     * @param data Pixel data (RGBA)
     * @param width Texture width
     * @param height Texture height
     * @return true if added successfully
     */
    bool AddTextureFromData(const std::string& name, const void* data, int width, int height);

    /**
     * @brief Remove a texture from the atlas
     */
    void RemoveTexture(const std::string& name);

    /**
     * @brief Get atlas entry by name
     */
    const AtlasEntry* GetEntry(const std::string& name) const;

    /**
     * @brief Get UV coordinates for a texture
     */
    AtlasUV GetUV(const std::string& name) const;

    /**
     * @brief Check if a texture is in the atlas
     */
    bool HasTexture(const std::string& name) const;

    /**
     * @brief Build/rebuild the atlas
     * @param forceRebuild Force complete rebuild even if not needed
     * @return true if build successful
     */
    bool Build(bool forceRebuild = false);

    /**
     * @brief Bind atlas texture
     * @param atlasIndex Which atlas to bind (for multiple atlases)
     * @param slot Texture slot to bind to
     */
    void Bind(int atlasIndex = 0, uint32_t slot = 0) const;

    /**
     * @brief Unbind atlas texture
     */
    void Unbind(uint32_t slot = 0) const;

    /**
     * @brief Get number of atlas textures
     */
    int GetAtlasCount() const { return static_cast<int>(m_atlases.size()); }

    /**
     * @brief Get atlas texture ID
     */
    uint32_t GetAtlasTexture(int index) const;

    /**
     * @brief Get atlas dimensions
     */
    glm::ivec2 GetAtlasSize(int index) const;

    /**
     * @brief Get total number of entries
     */
    size_t GetEntryCount() const { return m_entries.size(); }

    /**
     * @brief Get configuration
     */
    const TextureAtlasConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration (may require rebuild)
     */
    void SetConfig(const TextureAtlasConfig& config);

    /**
     * @brief Get statistics
     */
    struct Stats {
        int totalTextures = 0;
        int atlasCount = 0;
        int totalPixels = 0;
        int usedPixels = 0;
        float utilization = 0.0f;
        int textureBindsSaved = 0;

        void Reset() {
            totalTextures = atlasCount = totalPixels = usedPixels = 0;
            utilization = 0.0f;
            textureBindsSaved = 0;
        }
    };

    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Export atlas to file (for debugging)
     */
    bool ExportAtlas(int atlasIndex, const std::string& path) const;

    /**
     * @brief Create a sprite atlas from a grid
     * @param texture Source texture
     * @param columns Number of columns in grid
     * @param rows Number of rows in grid
     * @param names Names for each cell (left-to-right, top-to-bottom)
     */
    bool CreateFromGrid(const std::shared_ptr<Texture>& texture,
                        int columns, int rows,
                        const std::vector<std::string>& names);

    /**
     * @brief Transform mesh UVs for atlas
     * @param uvs Original UV coordinates
     * @param textureName Name of texture in atlas
     * @return Transformed UVs
     */
    std::vector<glm::vec2> TransformUVs(const std::vector<glm::vec2>& uvs,
                                         const std::string& textureName) const;

private:
    struct PendingTexture {
        std::string name;
        std::vector<uint8_t> data;
        int width = 0;
        int height = 0;
    };

    std::vector<Atlas> m_atlases;
    std::unordered_map<std::string, AtlasEntry> m_entries;
    std::vector<PendingTexture> m_pendingTextures;

    TextureAtlasConfig m_config;
    Stats m_stats;
    bool m_initialized = false;
    bool m_dirty = false;

    // Packing algorithm
    int Pack(Atlas& atlas, int width, int height);
    int FindNode(Atlas& atlas, int nodeIndex, int width, int height);
    void SplitNode(Atlas& atlas, int nodeIndex, int width, int height);

    // Atlas management
    Atlas& CreateAtlas();
    void UpdateAtlasTexture(Atlas& atlas);

    // Utility
    static int NextPowerOfTwo(int value);
    void SortPendingTextures();
    void UpdateStats();
};

/**
 * @brief Texture packer using shelf algorithm (for streaming)
 */
class ShelfPacker {
public:
    struct Shelf {
        int y = 0;
        int height = 0;
        int usedWidth = 0;
    };

    ShelfPacker(int width, int height);

    /**
     * @brief Pack a rectangle
     * @return Rectangle position, or (-1,-1) if failed
     */
    AtlasRect Pack(int width, int height);

    /**
     * @brief Reset packer
     */
    void Reset();

    /**
     * @brief Get utilization
     */
    float GetUtilization() const;

private:
    int m_width;
    int m_height;
    std::vector<Shelf> m_shelves;
    int m_usedArea = 0;
};

/**
 * @brief Compressed texture format support
 */
enum class CompressedFormat {
    None,
    DXT1,       // BC1 - RGB
    DXT5,       // BC3 - RGBA
    ETC1,       // Mobile RGB
    ETC2,       // Mobile RGBA
    ASTC_4x4,   // High quality
    ASTC_8x8    // High compression
};

/**
 * @brief Texture compression utilities
 */
class TextureCompressor {
public:
    /**
     * @brief Compress texture data
     * @param data RGBA pixel data
     * @param width Texture width
     * @param height Texture height
     * @param format Target format
     * @return Compressed data
     */
    static std::vector<uint8_t> Compress(const void* data, int width, int height,
                                          CompressedFormat format);

    /**
     * @brief Check if format is supported on current hardware
     */
    static bool IsFormatSupported(CompressedFormat format);

    /**
     * @brief Get OpenGL format for compressed format
     */
    static uint32_t GetGLFormat(CompressedFormat format);

    /**
     * @brief Calculate compressed data size
     */
    static size_t GetCompressedSize(int width, int height, CompressedFormat format);
};

/**
 * @brief Virtual texture system for streaming large textures
 */
class VirtualTexture {
public:
    VirtualTexture();
    ~VirtualTexture();

    /**
     * @brief Initialize virtual texture
     * @param pageSize Size of each page (e.g., 128x128)
     * @param physicalSize Size of physical texture cache
     */
    bool Initialize(int pageSize = 128, int physicalSize = 4096);

    /**
     * @brief Request a page to be loaded
     */
    void RequestPage(int pageX, int pageY, int mipLevel);

    /**
     * @brief Update page table
     */
    void Update();

    /**
     * @brief Get page table texture
     */
    uint32_t GetPageTableTexture() const { return m_pageTable; }

    /**
     * @brief Get physical texture
     */
    uint32_t GetPhysicalTexture() const { return m_physicalTexture; }

private:
    struct Page {
        int x = 0;
        int y = 0;
        int mipLevel = 0;
        bool loaded = false;
        int physicalX = -1;
        int physicalY = -1;
        uint64_t lastUsed = 0;
    };

    int m_pageSize = 128;
    int m_physicalSize = 4096;
    int m_pagesPerAxis = 32;

    uint32_t m_pageTable = 0;
    uint32_t m_physicalTexture = 0;

    std::vector<Page> m_pages;
    std::vector<glm::ivec2> m_freeSlots;
    bool m_initialized = false;
};

} // namespace Nova
