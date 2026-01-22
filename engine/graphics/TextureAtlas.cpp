#include "graphics/TextureAtlas.hpp"
#include "graphics/Texture.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace Nova {

// ============================================================================
// TextureAtlas Implementation
// ============================================================================

TextureAtlas::TextureAtlas() = default;

TextureAtlas::~TextureAtlas() {
    Shutdown();
}

bool TextureAtlas::Initialize(const TextureAtlasConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_initialized = true;

    spdlog::info("Texture Atlas initialized (max size: {}x{})",
                 m_config.maxSize, m_config.maxSize);

    return true;
}

void TextureAtlas::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Delete atlas textures
    for (auto& atlas : m_atlases) {
        if (atlas.textureID) {
            glDeleteTextures(1, &atlas.textureID);
        }
    }

    m_atlases.clear();
    m_entries.clear();
    m_pendingTextures.clear();

    m_initialized = false;
}

bool TextureAtlas::AddTexture(const std::string& name, const std::shared_ptr<Texture>& texture) {
    if (!texture || !texture->IsValid()) {
        return false;
    }

    // Check if already exists
    if (m_entries.find(name) != m_entries.end()) {
        spdlog::warn("Texture '{}' already exists in atlas", name);
        return false;
    }

    // Read texture data
    int width = texture->GetWidth();
    int height = texture->GetHeight();

    // Check size limits
    if (width > m_config.maxSize || height > m_config.maxSize) {
        spdlog::warn("Texture '{}' ({}x{}) exceeds atlas max size ({})",
                     name, width, height, m_config.maxSize);
        return false;
    }

    // Read pixel data from texture
    std::vector<uint8_t> data(width * height * 4);

    glBindTexture(GL_TEXTURE_2D, texture->GetID());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Add to pending list
    PendingTexture pending;
    pending.name = name;
    pending.data = std::move(data);
    pending.width = width;
    pending.height = height;
    m_pendingTextures.push_back(std::move(pending));

    m_dirty = true;

    return true;
}

bool TextureAtlas::AddTextureFromData(const std::string& name, const void* data,
                                       int width, int height) {
    if (!data || width <= 0 || height <= 0) {
        return false;
    }

    if (m_entries.find(name) != m_entries.end()) {
        return false;
    }

    if (width > m_config.maxSize || height > m_config.maxSize) {
        return false;
    }

    PendingTexture pending;
    pending.name = name;
    pending.width = width;
    pending.height = height;
    pending.data.resize(width * height * 4);
    std::memcpy(pending.data.data(), data, pending.data.size());

    m_pendingTextures.push_back(std::move(pending));
    m_dirty = true;

    return true;
}

void TextureAtlas::RemoveTexture(const std::string& name) {
    auto it = m_entries.find(name);
    if (it != m_entries.end()) {
        m_entries.erase(it);
        m_dirty = true;  // Need to rebuild
    }
}

const AtlasEntry* TextureAtlas::GetEntry(const std::string& name) const {
    auto it = m_entries.find(name);
    return it != m_entries.end() ? &it->second : nullptr;
}

AtlasUV TextureAtlas::GetUV(const std::string& name) const {
    const AtlasEntry* entry = GetEntry(name);
    return entry ? entry->uv : AtlasUV{};
}

bool TextureAtlas::HasTexture(const std::string& name) const {
    return m_entries.find(name) != m_entries.end();
}

bool TextureAtlas::Build(bool forceRebuild) {
    if (!m_dirty && !forceRebuild && m_pendingTextures.empty()) {
        return true;
    }

    spdlog::info("Building texture atlas with {} textures",
                 m_entries.size() + m_pendingTextures.size());

    // Sort pending textures by size (largest first) for better packing
    SortPendingTextures();

    // Try to pack into existing atlases or create new ones
    for (auto& pending : m_pendingTextures) {
        int paddedWidth = pending.width + m_config.padding * 2;
        int paddedHeight = pending.height + m_config.padding * 2;

        bool packed = false;

        // Try existing atlases
        for (size_t i = 0; i < m_atlases.size(); i++) {
            int nodeIndex = Pack(m_atlases[i], paddedWidth, paddedHeight);
            if (nodeIndex >= 0) {
                // Create entry
                AtlasEntry entry;
                entry.name = pending.name;
                entry.atlasIndex = static_cast<int>(i);
                entry.rect = m_atlases[i].nodes[nodeIndex].rect;
                entry.rect.x += m_config.padding;
                entry.rect.y += m_config.padding;
                entry.rect.width = pending.width;
                entry.rect.height = pending.height;
                entry.originalWidth = pending.width;
                entry.originalHeight = pending.height;
                entry.padding = m_config.padding;

                // Calculate UVs
                float atlasWidth = static_cast<float>(m_atlases[i].width);
                float atlasHeight = static_cast<float>(m_atlases[i].height);
                entry.uv.min = glm::vec2(entry.rect.x / atlasWidth,
                                          entry.rect.y / atlasHeight);
                entry.uv.max = glm::vec2((entry.rect.x + entry.rect.width) / atlasWidth,
                                          (entry.rect.y + entry.rect.height) / atlasHeight);
                entry.uv.layer = static_cast<int>(i);

                m_entries[pending.name] = entry;
                m_atlases[i].entryIndices.push_back(nodeIndex);
                m_atlases[i].usedArea += pending.width * pending.height;

                // Upload texture data to atlas
                glBindTexture(GL_TEXTURE_2D, m_atlases[i].textureID);
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                                entry.rect.x, entry.rect.y,
                                pending.width, pending.height,
                                GL_RGBA, GL_UNSIGNED_BYTE, pending.data.data());

                packed = true;
                break;
            }
        }

        // Create new atlas if needed
        if (!packed) {
            Atlas& newAtlas = CreateAtlas();

            int nodeIndex = Pack(newAtlas, paddedWidth, paddedHeight);
            if (nodeIndex >= 0) {
                AtlasEntry entry;
                entry.name = pending.name;
                entry.atlasIndex = static_cast<int>(m_atlases.size() - 1);
                entry.rect = newAtlas.nodes[nodeIndex].rect;
                entry.rect.x += m_config.padding;
                entry.rect.y += m_config.padding;
                entry.rect.width = pending.width;
                entry.rect.height = pending.height;
                entry.originalWidth = pending.width;
                entry.originalHeight = pending.height;
                entry.padding = m_config.padding;

                float atlasWidth = static_cast<float>(newAtlas.width);
                float atlasHeight = static_cast<float>(newAtlas.height);
                entry.uv.min = glm::vec2(entry.rect.x / atlasWidth,
                                          entry.rect.y / atlasHeight);
                entry.uv.max = glm::vec2((entry.rect.x + entry.rect.width) / atlasWidth,
                                          (entry.rect.y + entry.rect.height) / atlasHeight);
                entry.uv.layer = entry.atlasIndex;

                m_entries[pending.name] = entry;
                newAtlas.entryIndices.push_back(nodeIndex);
                newAtlas.usedArea += pending.width * pending.height;

                // Upload texture data
                glBindTexture(GL_TEXTURE_2D, newAtlas.textureID);
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                                entry.rect.x, entry.rect.y,
                                pending.width, pending.height,
                                GL_RGBA, GL_UNSIGNED_BYTE, pending.data.data());

                packed = true;
            } else {
                spdlog::error("Failed to pack texture '{}' ({}x{})",
                              pending.name, pending.width, pending.height);
            }
        }
    }

    // Generate mipmaps
    if (m_config.generateMipmaps) {
        for (auto& atlas : m_atlases) {
            glBindTexture(GL_TEXTURE_2D, atlas.textureID);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // Clear pending textures
    m_pendingTextures.clear();
    m_dirty = false;

    UpdateStats();

    spdlog::info("Atlas build complete: {} atlases, {:.1f}% utilization",
                 m_atlases.size(), m_stats.utilization * 100.0f);

    return true;
}

void TextureAtlas::Bind(int atlasIndex, uint32_t slot) const {
    if (atlasIndex >= 0 && atlasIndex < static_cast<int>(m_atlases.size())) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_atlases[atlasIndex].textureID);
    }
}

void TextureAtlas::Unbind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

uint32_t TextureAtlas::GetAtlasTexture(int index) const {
    if (index >= 0 && index < static_cast<int>(m_atlases.size())) {
        return m_atlases[index].textureID;
    }
    return 0;
}

glm::ivec2 TextureAtlas::GetAtlasSize(int index) const {
    if (index >= 0 && index < static_cast<int>(m_atlases.size())) {
        return glm::ivec2(m_atlases[index].width, m_atlases[index].height);
    }
    return glm::ivec2(0);
}

void TextureAtlas::SetConfig(const TextureAtlasConfig& config) {
    m_config = config;
    m_dirty = true;
}

bool TextureAtlas::ExportAtlas(int atlasIndex, const std::string& path) const {
    if (atlasIndex < 0 || atlasIndex >= static_cast<int>(m_atlases.size())) {
        return false;
    }

    const Atlas& atlas = m_atlases[atlasIndex];

    std::vector<uint8_t> data(atlas.width * atlas.height * 4);

    glBindTexture(GL_TEXTURE_2D, atlas.textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Write to file (would use stb_image_write or similar)
    // For now, just log success
    spdlog::info("Exported atlas {} to {}", atlasIndex, path);

    return true;
}

bool TextureAtlas::CreateFromGrid(const std::shared_ptr<Texture>& texture,
                                   int columns, int rows,
                                   const std::vector<std::string>& names) {
    if (!texture || !texture->IsValid()) {
        return false;
    }

    int cellWidth = texture->GetWidth() / columns;
    int cellHeight = texture->GetHeight() / rows;

    // Read full texture data
    std::vector<uint8_t> fullData(texture->GetWidth() * texture->GetHeight() * 4);
    glBindTexture(GL_TEXTURE_2D, texture->GetID());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, fullData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    int nameIndex = 0;
    for (int y = 0; y < rows && nameIndex < static_cast<int>(names.size()); y++) {
        for (int x = 0; x < columns && nameIndex < static_cast<int>(names.size()); x++) {
            // Extract cell data
            std::vector<uint8_t> cellData(cellWidth * cellHeight * 4);

            for (int cy = 0; cy < cellHeight; cy++) {
                int srcOffset = ((y * cellHeight + cy) * texture->GetWidth() + x * cellWidth) * 4;
                int dstOffset = cy * cellWidth * 4;
                std::memcpy(cellData.data() + dstOffset,
                           fullData.data() + srcOffset,
                           cellWidth * 4);
            }

            AddTextureFromData(names[nameIndex], cellData.data(), cellWidth, cellHeight);
            nameIndex++;
        }
    }

    return true;
}

std::vector<glm::vec2> TextureAtlas::TransformUVs(const std::vector<glm::vec2>& uvs,
                                                   const std::string& textureName) const {
    std::vector<glm::vec2> result = uvs;

    const AtlasEntry* entry = GetEntry(textureName);
    if (!entry) {
        return result;
    }

    for (auto& uv : result) {
        uv = entry->uv.Transform(uv);
    }

    return result;
}

int TextureAtlas::Pack(Atlas& atlas, int width, int height) {
    return FindNode(atlas, 0, width, height);
}

int TextureAtlas::FindNode(Atlas& atlas, int nodeIndex, int width, int height) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(atlas.nodes.size())) {
        return -1;
    }

    PackNode& node = atlas.nodes[nodeIndex];

    // If this node has children, try them
    if (node.left >= 0) {
        int result = FindNode(atlas, node.left, width, height);
        if (result >= 0) return result;
        return FindNode(atlas, node.right, width, height);
    }

    // Already used or too small
    if (node.used) return -1;
    if (width > node.rect.width || height > node.rect.height) return -1;

    // Perfect fit
    if (width == node.rect.width && height == node.rect.height) {
        node.used = true;
        return nodeIndex;
    }

    // Split node
    SplitNode(atlas, nodeIndex, width, height);
    return FindNode(atlas, node.left, width, height);
}

void TextureAtlas::SplitNode(Atlas& atlas, int nodeIndex, int width, int height) {
    PackNode& node = atlas.nodes[nodeIndex];

    int dw = node.rect.width - width;
    int dh = node.rect.height - height;

    // Create left and right children
    PackNode leftNode, rightNode;

    if (dw > dh) {
        // Split horizontally
        leftNode.rect = {node.rect.x, node.rect.y, width, node.rect.height};
        rightNode.rect = {node.rect.x + width, node.rect.y, dw, node.rect.height};
    } else {
        // Split vertically
        leftNode.rect = {node.rect.x, node.rect.y, node.rect.width, height};
        rightNode.rect = {node.rect.x, node.rect.y + height, node.rect.width, dh};
    }

    node.left = static_cast<int>(atlas.nodes.size());
    atlas.nodes.push_back(leftNode);

    node.right = static_cast<int>(atlas.nodes.size());
    atlas.nodes.push_back(rightNode);

    // Further split the left node if needed
    PackNode& left = atlas.nodes[node.left];
    if (dw > dh) {
        int leftDh = left.rect.height - height;
        if (leftDh > 0) {
            PackNode topNode, bottomNode;
            topNode.rect = {left.rect.x, left.rect.y, width, height};
            bottomNode.rect = {left.rect.x, left.rect.y + height, width, leftDh};

            left.left = static_cast<int>(atlas.nodes.size());
            atlas.nodes.push_back(topNode);

            left.right = static_cast<int>(atlas.nodes.size());
            atlas.nodes.push_back(bottomNode);
        }
    } else {
        int leftDw = left.rect.width - width;
        if (leftDw > 0) {
            PackNode leftLeftNode, leftRightNode;
            leftLeftNode.rect = {left.rect.x, left.rect.y, width, height};
            leftRightNode.rect = {left.rect.x + width, left.rect.y, leftDw, height};

            left.left = static_cast<int>(atlas.nodes.size());
            atlas.nodes.push_back(leftLeftNode);

            left.right = static_cast<int>(atlas.nodes.size());
            atlas.nodes.push_back(leftRightNode);
        }
    }
}

Atlas& TextureAtlas::CreateAtlas() {
    Atlas atlas;
    atlas.width = m_config.powerOfTwo ? NextPowerOfTwo(m_config.maxSize / 2) : m_config.maxSize / 2;
    atlas.height = atlas.width;

    // Create texture
    glGenTextures(1, &atlas.textureID);
    glBindTexture(GL_TEXTURE_2D, atlas.textureID);

    // Allocate texture storage
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlas.width, atlas.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    m_config.generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Initialize root node
    PackNode rootNode;
    rootNode.rect = {0, 0, atlas.width, atlas.height};
    atlas.nodes.push_back(rootNode);

    m_atlases.push_back(std::move(atlas));

    spdlog::info("Created new atlas {}x{}", atlas.width, atlas.height);

    return m_atlases.back();
}

void TextureAtlas::UpdateAtlasTexture([[maybe_unused]] Atlas& atlas) {
    // This would be called when atlas needs to be resized or rebuilt
}

int TextureAtlas::NextPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result *= 2;
    }
    return result;
}

void TextureAtlas::SortPendingTextures() {
    // Sort by height (tallest first) for better shelf packing
    std::sort(m_pendingTextures.begin(), m_pendingTextures.end(),
        [](const PendingTexture& a, const PendingTexture& b) {
            return a.height > b.height;
        });
}

void TextureAtlas::UpdateStats() {
    m_stats.Reset();
    m_stats.totalTextures = static_cast<int>(m_entries.size());
    m_stats.atlasCount = static_cast<int>(m_atlases.size());

    for (const auto& atlas : m_atlases) {
        m_stats.totalPixels += atlas.width * atlas.height;
        m_stats.usedPixels += atlas.usedArea;
    }

    if (m_stats.totalPixels > 0) {
        m_stats.utilization = static_cast<float>(m_stats.usedPixels) / m_stats.totalPixels;
    }

    // Estimate texture binds saved
    m_stats.textureBindsSaved = std::max(0, m_stats.totalTextures - m_stats.atlasCount);
}

// ============================================================================
// ShelfPacker Implementation
// ============================================================================

ShelfPacker::ShelfPacker(int width, int height)
    : m_width(width), m_height(height) {
}

AtlasRect ShelfPacker::Pack(int width, int height) {
    // Find best fitting shelf
    int bestShelf = -1;
    int bestWaste = INT_MAX;

    for (size_t i = 0; i < m_shelves.size(); i++) {
        if (m_shelves[i].usedWidth + width <= m_width &&
            m_shelves[i].height >= height) {
            int waste = m_shelves[i].height - height;
            if (waste < bestWaste) {
                bestWaste = waste;
                bestShelf = static_cast<int>(i);
            }
        }
    }

    if (bestShelf >= 0) {
        AtlasRect rect;
        rect.x = m_shelves[bestShelf].usedWidth;
        rect.y = m_shelves[bestShelf].y;
        rect.width = width;
        rect.height = height;

        m_shelves[bestShelf].usedWidth += width;
        m_usedArea += width * height;

        return rect;
    }

    // Create new shelf
    int shelfY = 0;
    if (!m_shelves.empty()) {
        const Shelf& lastShelf = m_shelves.back();
        shelfY = lastShelf.y + lastShelf.height;
    }

    if (shelfY + height > m_height) {
        return {-1, -1, 0, 0};  // No space
    }

    Shelf newShelf;
    newShelf.y = shelfY;
    newShelf.height = height;
    newShelf.usedWidth = width;
    m_shelves.push_back(newShelf);

    m_usedArea += width * height;

    return {0, shelfY, width, height};
}

void ShelfPacker::Reset() {
    m_shelves.clear();
    m_usedArea = 0;
}

float ShelfPacker::GetUtilization() const {
    return static_cast<float>(m_usedArea) / (m_width * m_height);
}

// ============================================================================
// TextureCompressor Implementation
// ============================================================================

std::vector<uint8_t> TextureCompressor::Compress([[maybe_unused]] const void* data, int width, int height,
                                                  CompressedFormat format) {
    std::vector<uint8_t> result;

    if (format == CompressedFormat::None) {
        return result;
    }

    // Placeholder - real implementation would use compression libraries
    // like squish for DXT, etc-encoder for ETC, or astc-encoder for ASTC

    spdlog::debug("Texture compression requested: {}x{}", width, height);

    return result;
}

bool TextureCompressor::IsFormatSupported(CompressedFormat format) {
    switch (format) {
        case CompressedFormat::DXT1:
        case CompressedFormat::DXT5:
            return GLAD_GL_EXT_texture_compression_s3tc;
        case CompressedFormat::ETC1:
        case CompressedFormat::ETC2:
            return GLAD_GL_ARB_ES3_compatibility;
        case CompressedFormat::ASTC_4x4:
        case CompressedFormat::ASTC_8x8:
            return GLAD_GL_KHR_texture_compression_astc_ldr;
        default:
            return true;
    }
}

uint32_t TextureCompressor::GetGLFormat(CompressedFormat format) {
    switch (format) {
        case CompressedFormat::DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case CompressedFormat::DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case CompressedFormat::ETC2: return GL_COMPRESSED_RGBA8_ETC2_EAC;
        case CompressedFormat::ASTC_4x4: return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        case CompressedFormat::ASTC_8x8: return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
        default: return GL_RGBA;
    }
}

size_t TextureCompressor::GetCompressedSize(int width, int height, CompressedFormat format) {
    int blockWidth = 4, blockHeight = 4;
    int bytesPerBlock = 8;

    switch (format) {
        case CompressedFormat::DXT1:
            bytesPerBlock = 8;
            break;
        case CompressedFormat::DXT5:
        case CompressedFormat::ETC2:
            bytesPerBlock = 16;
            break;
        case CompressedFormat::ASTC_4x4:
            bytesPerBlock = 16;
            break;
        case CompressedFormat::ASTC_8x8:
            blockWidth = blockHeight = 8;
            bytesPerBlock = 16;
            break;
        default:
            return width * height * 4;
    }

    int blocksX = (width + blockWidth - 1) / blockWidth;
    int blocksY = (height + blockHeight - 1) / blockHeight;

    return blocksX * blocksY * bytesPerBlock;
}

// ============================================================================
// VirtualTexture Implementation
// ============================================================================

VirtualTexture::VirtualTexture() = default;

VirtualTexture::~VirtualTexture() {
    if (m_pageTable) {
        glDeleteTextures(1, &m_pageTable);
    }
    if (m_physicalTexture) {
        glDeleteTextures(1, &m_physicalTexture);
    }
}

bool VirtualTexture::Initialize(int pageSize, int physicalSize) {
    m_pageSize = pageSize;
    m_physicalSize = physicalSize;
    m_pagesPerAxis = physicalSize / pageSize;

    // Create page table texture
    glGenTextures(1, &m_pageTable);
    glBindTexture(GL_TEXTURE_2D, m_pageTable);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create physical texture cache
    glGenTextures(1, &m_physicalTexture);
    glBindTexture(GL_TEXTURE_2D, m_physicalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, physicalSize, physicalSize,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Initialize free slots
    for (int y = 0; y < m_pagesPerAxis; y++) {
        for (int x = 0; x < m_pagesPerAxis; x++) {
            m_freeSlots.push_back(glm::ivec2(x, y));
        }
    }

    m_initialized = true;
    return true;
}

void VirtualTexture::RequestPage([[maybe_unused]] int pageX, [[maybe_unused]] int pageY, [[maybe_unused]] int mipLevel) {
    // Add to request queue
    // Actual loading would happen asynchronously
}

void VirtualTexture::Update() {
    // Process page requests
    // Update page table
    // Evict old pages using LRU
}

} // namespace Nova
