#include "TextureImporter.hpp"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>

// Note: In a real implementation, you would include:
// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"
// #define STB_IMAGE_RESIZE_IMPLEMENTATION
// #include "stb_image_resize2.h"

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Constructor/Destructor
// ============================================================================

TextureImporter::TextureImporter() = default;
TextureImporter::~TextureImporter() = default;

// ============================================================================
// Single Texture Import
// ============================================================================

ImportedTexture TextureImporter::Import(const std::string& path,
                                         const TextureImportSettings& settings,
                                         ImportProgress* progress) {
    ImportedTexture result;
    result.sourcePath = path;
    result.success = false;

    // Validate file exists
    if (!fs::exists(path)) {
        result.errorMessage = "File not found: " + path;
        if (progress) progress->Error(result.errorMessage);
        return result;
    }

    // Setup progress stages
    if (progress) {
        progress->AddStage("load", "Loading image", 1.0f);
        progress->AddStage("process", "Processing", 2.0f);
        progress->AddStage("compress", "Compressing", 3.0f);
        progress->AddStage("output", "Generating output", 1.0f);
        progress->SetStatus(ImportStatus::InProgress);
        progress->StartTiming();
    }

    // Load image
    if (progress) progress->BeginStage("load");
    if (m_loadCallback) m_loadCallback(path);

    ImageData image = LoadImage(path);
    if (image.pixels.empty() && image.hdrPixels.empty()) {
        result.errorMessage = "Failed to load image: " + path;
        if (progress) {
            progress->Error(result.errorMessage);
            progress->SetStatus(ImportStatus::Failed);
        }
        return result;
    }

    result.width = image.width;
    result.height = image.height;
    result.channels = image.channels;
    result.originalSize = image.pixels.size();

    if (progress) progress->EndStage();

    // Check for cancellation
    if (progress && progress->IsCancellationRequested()) {
        progress->MarkCancelled();
        return result;
    }

    // Process image
    if (progress) progress->BeginStage("process");

    // Auto-detect texture type
    TextureImportSettings effectiveSettings = settings;
    if (effectiveSettings.textureType == TextureType::Default) {
        effectiveSettings.AutoDetectType(path);
        result.detectedType = effectiveSettings.textureType;
    }

    // Apply transforms
    if (effectiveSettings.flipVertically) {
        FlipVertical(image);
    }
    if (effectiveSettings.flipHorizontally) {
        FlipHorizontal(image);
    }

    // Resize if needed
    bool needsResize = false;
    int targetWidth = image.width;
    int targetHeight = image.height;

    if (image.width > effectiveSettings.maxWidth) {
        targetWidth = effectiveSettings.maxWidth;
        needsResize = true;
    }
    if (image.height > effectiveSettings.maxHeight) {
        targetHeight = effectiveSettings.maxHeight;
        needsResize = true;
    }

    if (effectiveSettings.powerOfTwo) {
        if (!IsPowerOfTwo(targetWidth)) {
            targetWidth = NextPowerOfTwo(targetWidth);
            needsResize = true;
        }
        if (!IsPowerOfTwo(targetHeight)) {
            targetHeight = NextPowerOfTwo(targetHeight);
            needsResize = true;
        }
    }

    if (needsResize) {
        image = Resize(image, targetWidth, targetHeight);
        result.width = image.width;
        result.height = image.height;
        if (progress) progress->Info("Resized to " + std::to_string(targetWidth) + "x" + std::to_string(targetHeight));
    }

    // Normal map processing
    if (effectiveSettings.isNormalMap || effectiveSettings.textureType == TextureType::Normal) {
        if (effectiveSettings.normalMapFromHeight) {
            image = HeightToNormal(image, effectiveSettings.normalMapStrength);
            if (progress) progress->Info("Generated normal map from height");
        }
        if (effectiveSettings.reconstructZ) {
            ReconstructNormalZ(image);
        }
        NormalizeNormalMap(image);
    }

    // Alpha processing
    if (effectiveSettings.premultiplyAlpha && image.channels == 4) {
        PremultiplyAlpha(image);
    }

    // Color space conversion
    result.sRGB = effectiveSettings.sRGB;

    if (progress) progress->EndStage();

    // Compression
    if (progress) progress->BeginStage("compress");

    TextureCompression targetCompression = effectiveSettings.compression;
    if (effectiveSettings.isNormalMap && targetCompression == TextureCompression::BC7) {
        targetCompression = TextureCompression::BC5;
    }

    result.compression = targetCompression;
    result.compressedData = Compress(image, targetCompression,
                                      effectiveSettings.compressionQuality,
                                      effectiveSettings.generateMipmaps);

    result.hasMipmaps = effectiveSettings.generateMipmaps;
    result.mipmapCount = static_cast<int>(result.compressedData.mipmaps.size());

    // Calculate sizes
    result.compressedSize = 0;
    for (const auto& mip : result.compressedData.mipmaps) {
        result.compressedSize += mip.dataSize;
    }
    result.compressionRatio = result.originalSize > 0 ?
        static_cast<float>(result.compressedSize) / result.originalSize : 1.0f;

    if (progress) progress->EndStage();

    // Generate output
    if (progress) progress->BeginStage("output");

    // Sprite slicing
    if (effectiveSettings.sliceSprites) {
        result.sprites = SliceSpriteSheet(image,
            effectiveSettings.sliceWidth,
            effectiveSettings.sliceHeight,
            effectiveSettings.sliceColumns,
            effectiveSettings.sliceRows);
        if (progress) progress->Info("Sliced into " + std::to_string(result.sprites.size()) + " sprites");
    }

    // Thumbnail
    if (effectiveSettings.generateThumbnail) {
        ImageData thumb = GenerateThumbnail(image, effectiveSettings.thumbnailSize);
        result.thumbnail = std::move(thumb.pixels);
        result.thumbnailWidth = thumb.width;
        result.thumbnailHeight = thumb.height;
    }

    // Generate output path
    if (effectiveSettings.outputPath.empty()) {
        result.outputPath = path + ".nova";
    } else {
        result.outputPath = effectiveSettings.outputPath;
    }

    if (progress) progress->EndStage();

    // Success
    result.success = true;
    if (progress) {
        if (progress->HasWarnings()) {
            progress->SetStatus(ImportStatus::CompletedWithWarnings);
        } else {
            progress->SetStatus(ImportStatus::Completed);
        }
        progress->StopTiming();
    }

    return result;
}

ImportedTexture TextureImporter::Import(const std::string& path) {
    TextureImportSettings settings;
    settings.AutoDetectType(path);
    return Import(path, settings);
}

std::unique_ptr<Texture> TextureImporter::QuickImport(const std::string& path) {
    ImportedTexture imported = Import(path);
    if (!imported.success) {
        return nullptr;
    }

    // Would create actual Texture here
    // auto texture = std::make_unique<Texture>();
    // texture->Load(path, imported.sRGB, imported.hasMipmaps);
    // return texture;

    return nullptr;  // Placeholder
}

// ============================================================================
// Batch Import
// ============================================================================

std::vector<ImportedTexture> TextureImporter::ImportBatch(
    const std::vector<std::string>& paths,
    const TextureImportSettings& settings,
    ImportProgressTracker* tracker) {

    std::vector<ImportedTexture> results;
    results.reserve(paths.size());

    for (const auto& path : paths) {
        ImportProgress* progress = tracker ? tracker->AddImport(path) : nullptr;
        results.push_back(Import(path, settings, progress));

        if (tracker && tracker->IsAllCompleted()) {
            break;  // Cancelled
        }
    }

    return results;
}

std::vector<ImportedTexture> TextureImporter::ImportBatch(
    const std::vector<std::pair<std::string, TextureImportSettings>>& imports,
    ImportProgressTracker* tracker) {

    std::vector<ImportedTexture> results;
    results.reserve(imports.size());

    for (const auto& [path, settings] : imports) {
        ImportProgress* progress = tracker ? tracker->AddImport(path) : nullptr;
        results.push_back(Import(path, settings, progress));

        if (tracker && tracker->IsAllCompleted()) {
            break;
        }
    }

    return results;
}

// ============================================================================
// Image Loading
// ============================================================================

ImageData TextureImporter::LoadImage(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".hdr" || ext == ".exr") {
        return LoadHDRImage(path);
    } else if (ext == ".png") {
        return LoadPNG(path);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        return LoadJPG(path);
    } else if (ext == ".tga") {
        return LoadTGA(path);
    } else if (ext == ".bmp") {
        return LoadBMP(path);
    } else if (ext == ".dds") {
        return LoadDDS(path);
    } else if (ext == ".ktx") {
        return LoadKTX(path);
    }

    // Fallback - try as generic using stb_image pattern
    return LoadPNG(path);  // PNG loader usually handles most formats
}

ImageData TextureImporter::LoadHDRImage(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // Read HDR header
    std::string line;
    int width = 0, height = 0;
    bool foundFormat = false;

    while (std::getline(file, line)) {
        if (line.empty()) break;
        if (line.find("FORMAT=32-bit_rle_rgbe") != std::string::npos) {
            foundFormat = true;
        }
        if (line.find("-Y") != std::string::npos && line.find("+X") != std::string::npos) {
            sscanf(line.c_str(), "-Y %d +X %d", &height, &width);
        }
    }

    if (!foundFormat || width == 0 || height == 0) {
        return result;
    }

    result.width = width;
    result.height = height;
    result.channels = 3;
    result.isHDR = true;
    result.hdrPixels.resize(width * height * 3);

    // Simplified HDR loading (real implementation would decode RGBE)
    // For demonstration, fill with placeholder
    std::fill(result.hdrPixels.begin(), result.hdrPixels.end(), 0.5f);

    return result;
}

ImageData TextureImporter::LoadPNG(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // Check PNG signature
    uint8_t signature[8];
    file.read(reinterpret_cast<char*>(signature), 8);
    const uint8_t pngSignature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(signature, pngSignature, 8) != 0) {
        return result;
    }

    // Parse chunks (simplified - real implementation would use libpng or stb_image)
    while (!file.eof()) {
        uint32_t length;
        char type[4];
        file.read(reinterpret_cast<char*>(&length), 4);
        file.read(type, 4);

        // Convert big-endian to native
        length = ((length >> 24) & 0xFF) | ((length >> 8) & 0xFF00) |
                 ((length << 8) & 0xFF0000) | ((length << 24) & 0xFF000000);

        if (memcmp(type, "IHDR", 4) == 0) {
            uint32_t w, h;
            uint8_t bitDepth, colorType;
            file.read(reinterpret_cast<char*>(&w), 4);
            file.read(reinterpret_cast<char*>(&h), 4);
            file.read(reinterpret_cast<char*>(&bitDepth), 1);
            file.read(reinterpret_cast<char*>(&colorType), 1);

            // Convert big-endian
            w = ((w >> 24) & 0xFF) | ((w >> 8) & 0xFF00) |
                ((w << 8) & 0xFF0000) | ((w << 24) & 0xFF000000);
            h = ((h >> 24) & 0xFF) | ((h >> 8) & 0xFF00) |
                ((h << 8) & 0xFF0000) | ((h << 24) & 0xFF000000);

            result.width = w;
            result.height = h;

            switch (colorType) {
                case 0: result.channels = 1; break;  // Grayscale
                case 2: result.channels = 3; break;  // RGB
                case 4: result.channels = 2; break;  // Grayscale+Alpha
                case 6: result.channels = 4; break;  // RGBA
                default: result.channels = 4; break;
            }

            result.is16Bit = (bitDepth == 16);

            file.seekg(length - 6 + 4, std::ios::cur);  // Skip rest + CRC
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        } else {
            file.seekg(length + 4, std::ios::cur);  // Skip chunk data + CRC
        }
    }

    // For real implementation, decode the image data
    // Here we create placeholder data
    if (result.width > 0 && result.height > 0) {
        result.pixels.resize(result.width * result.height * result.channels);
        // In production, use stb_image or libpng to actually decode
        std::fill(result.pixels.begin(), result.pixels.end(), 128);
    }

    return result;
}

ImageData TextureImporter::LoadJPG(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // Check JPEG signature (SOI marker)
    uint8_t marker[2];
    file.read(reinterpret_cast<char*>(marker), 2);
    if (marker[0] != 0xFF || marker[1] != 0xD8) {
        return result;
    }

    // Parse markers to find SOF0 (Start Of Frame)
    while (!file.eof()) {
        uint8_t ff, type;
        file.read(reinterpret_cast<char*>(&ff), 1);
        if (ff != 0xFF) continue;

        file.read(reinterpret_cast<char*>(&type), 1);

        if (type == 0xC0 || type == 0xC2) {  // SOF0 or SOF2
            uint16_t length;
            uint8_t precision;
            uint16_t height, width;
            uint8_t components;

            file.read(reinterpret_cast<char*>(&length), 2);
            file.read(reinterpret_cast<char*>(&precision), 1);
            file.read(reinterpret_cast<char*>(&height), 2);
            file.read(reinterpret_cast<char*>(&width), 2);
            file.read(reinterpret_cast<char*>(&components), 1);

            // Convert big-endian
            height = ((height >> 8) & 0xFF) | ((height << 8) & 0xFF00);
            width = ((width >> 8) & 0xFF) | ((width << 8) & 0xFF00);

            result.width = width;
            result.height = height;
            result.channels = components;
            break;
        } else if (type == 0xD9) {  // EOI
            break;
        } else if (type >= 0xD0 && type <= 0xD7) {  // RST markers
            continue;
        } else if (type == 0x01 || type == 0x00) {  // TEM or padding
            continue;
        } else {
            // Skip segment
            uint16_t length;
            file.read(reinterpret_cast<char*>(&length), 2);
            length = ((length >> 8) & 0xFF) | ((length << 8) & 0xFF00);
            file.seekg(length - 2, std::ios::cur);
        }
    }

    // Create placeholder data (real implementation would use libjpeg or stb_image)
    if (result.width > 0 && result.height > 0) {
        result.pixels.resize(result.width * result.height * result.channels);
        std::fill(result.pixels.begin(), result.pixels.end(), 128);
    }

    return result;
}

ImageData TextureImporter::LoadTGA(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // TGA header
    struct TGAHeader {
        uint8_t idLength;
        uint8_t colorMapType;
        uint8_t imageType;
        uint16_t colorMapOrigin;
        uint16_t colorMapLength;
        uint8_t colorMapDepth;
        uint16_t xOrigin;
        uint16_t yOrigin;
        uint16_t width;
        uint16_t height;
        uint8_t bitsPerPixel;
        uint8_t imageDescriptor;
    } header;

    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    result.width = header.width;
    result.height = header.height;
    result.channels = header.bitsPerPixel / 8;

    // Skip ID field
    file.seekg(header.idLength, std::ios::cur);

    // Read pixel data
    size_t dataSize = result.width * result.height * result.channels;
    result.pixels.resize(dataSize);

    if (header.imageType == 2 || header.imageType == 3) {
        // Uncompressed
        file.read(reinterpret_cast<char*>(result.pixels.data()), dataSize);

        // Convert BGR(A) to RGB(A)
        if (result.channels >= 3) {
            for (size_t i = 0; i < dataSize; i += result.channels) {
                std::swap(result.pixels[i], result.pixels[i + 2]);
            }
        }
    } else if (header.imageType == 10 || header.imageType == 11) {
        // RLE compressed - simplified placeholder
        std::fill(result.pixels.begin(), result.pixels.end(), 128);
    }

    // Handle vertical flip based on image descriptor
    if (!(header.imageDescriptor & 0x20)) {
        FlipVertical(result);
    }

    return result;
}

ImageData TextureImporter::LoadBMP(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // BMP header
    uint8_t signature[2];
    file.read(reinterpret_cast<char*>(signature), 2);
    if (signature[0] != 'B' || signature[1] != 'M') {
        return result;
    }

    file.seekg(18, std::ios::beg);  // Skip to width/height

    int32_t width, height;
    uint16_t planes, bitsPerPixel;

    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);
    file.read(reinterpret_cast<char*>(&planes), 2);
    file.read(reinterpret_cast<char*>(&bitsPerPixel), 2);

    result.width = std::abs(width);
    result.height = std::abs(height);
    result.channels = bitsPerPixel / 8;

    // Read data offset
    file.seekg(10, std::ios::beg);
    uint32_t dataOffset;
    file.read(reinterpret_cast<char*>(&dataOffset), 4);

    // Read pixel data
    file.seekg(dataOffset, std::ios::beg);
    int rowSize = ((result.width * result.channels + 3) / 4) * 4;  // 4-byte aligned

    result.pixels.resize(result.width * result.height * result.channels);

    for (int y = result.height - 1; y >= 0; --y) {  // BMP is bottom-up
        file.read(reinterpret_cast<char*>(result.pixels.data() + y * result.width * result.channels),
                  result.width * result.channels);
        file.seekg(rowSize - result.width * result.channels, std::ios::cur);  // Skip padding
    }

    // Convert BGR(A) to RGB(A)
    if (result.channels >= 3) {
        for (size_t i = 0; i < result.pixels.size(); i += result.channels) {
            std::swap(result.pixels[i], result.pixels[i + 2]);
        }
    }

    return result;
}

ImageData TextureImporter::LoadDDS(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // DDS magic number
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x20534444) {  // "DDS "
        return result;
    }

    // DDS header (simplified)
    file.seekg(12, std::ios::cur);  // Skip to height/width
    uint32_t height, width;
    file.read(reinterpret_cast<char*>(&height), 4);
    file.read(reinterpret_cast<char*>(&width), 4);

    result.width = width;
    result.height = height;
    result.channels = 4;  // DDS is typically RGBA

    // For full implementation, parse pixel format and decode compressed data
    result.pixels.resize(result.width * result.height * result.channels);
    std::fill(result.pixels.begin(), result.pixels.end(), 128);

    return result;
}

ImageData TextureImporter::LoadKTX(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // KTX identifier
    uint8_t identifier[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31,
                              0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    uint8_t readId[12];
    file.read(reinterpret_cast<char*>(readId), 12);

    if (memcmp(identifier, readId, 12) != 0) {
        return result;
    }

    // Parse KTX header
    file.seekg(36, std::ios::beg);  // Skip to pixelWidth
    uint32_t width, height;
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);

    result.width = width;
    result.height = height;
    result.channels = 4;

    // Placeholder
    result.pixels.resize(result.width * result.height * result.channels);
    std::fill(result.pixels.begin(), result.pixels.end(), 128);

    return result;
}

ImageData TextureImporter::LoadEXR(const std::string& path) {
    ImageData result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return result;
    }

    // EXR magic number
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x01312F76) {
        return result;
    }

    result.isHDR = true;
    // EXR parsing is complex - placeholder
    result.width = 256;
    result.height = 256;
    result.channels = 3;
    result.hdrPixels.resize(result.width * result.height * 3);
    std::fill(result.hdrPixels.begin(), result.hdrPixels.end(), 0.5f);

    return result;
}

bool TextureImporter::HasAlpha(const ImageData& image) {
    if (image.channels != 4) return false;

    for (size_t i = 3; i < image.pixels.size(); i += 4) {
        if (image.pixels[i] < 255) {
            return true;
        }
    }
    return false;
}

bool TextureImporter::GetImageInfo(const std::string& path, int& width, int& height, int& channels) {
    ImageData info = LoadImage(path);
    if (info.width == 0) return false;

    width = info.width;
    height = info.height;
    channels = info.channels;
    return true;
}

// ============================================================================
// Mipmap Generation
// ============================================================================

std::vector<MipmapLevel> TextureImporter::GenerateMipmaps(const ImageData& image,
                                                           MipmapQuality quality,
                                                           int maxLevels) {
    std::vector<MipmapLevel> mipmaps;

    int levels = maxLevels > 0 ? maxLevels : CalculateMipLevels(image.width, image.height);

    // Level 0 is the full image
    MipmapLevel level0;
    level0.width = image.width;
    level0.height = image.height;
    level0.data = image.pixels;
    level0.dataSize = image.pixels.size();
    mipmaps.push_back(level0);

    // Generate subsequent levels
    const uint8_t* srcData = image.pixels.data();
    int srcWidth = image.width;
    int srcHeight = image.height;

    for (int level = 1; level < levels; ++level) {
        MipmapLevel mip = GenerateMipLevel(srcData, srcWidth, srcHeight, image.channels, quality);

        if (mip.width == 0) break;

        mipmaps.push_back(mip);

        srcData = mipmaps.back().data.data();
        srcWidth = mip.width;
        srcHeight = mip.height;

        if (srcWidth == 1 && srcHeight == 1) break;
    }

    return mipmaps;
}

int TextureImporter::CalculateMipLevels(int width, int height) {
    int maxDim = std::max(width, height);
    return static_cast<int>(std::floor(std::log2(maxDim))) + 1;
}

MipmapLevel TextureImporter::GenerateMipLevel(const uint8_t* srcData, int srcWidth, int srcHeight,
                                               int channels, MipmapQuality quality) {
    MipmapLevel mip;

    mip.width = std::max(1, srcWidth / 2);
    mip.height = std::max(1, srcHeight / 2);
    mip.data.resize(mip.width * mip.height * channels);
    mip.dataSize = mip.data.size();

    // Box filter (average 2x2 blocks)
    for (int y = 0; y < mip.height; ++y) {
        for (int x = 0; x < mip.width; ++x) {
            int srcX = x * 2;
            int srcY = y * 2;

            for (int c = 0; c < channels; ++c) {
                int sum = 0;
                int count = 0;

                // Sample 2x2 block
                for (int dy = 0; dy < 2 && srcY + dy < srcHeight; ++dy) {
                    for (int dx = 0; dx < 2 && srcX + dx < srcWidth; ++dx) {
                        int idx = ((srcY + dy) * srcWidth + (srcX + dx)) * channels + c;
                        sum += srcData[idx];
                        count++;
                    }
                }

                int dstIdx = (y * mip.width + x) * channels + c;
                mip.data[dstIdx] = static_cast<uint8_t>(sum / count);
            }
        }
    }

    // Apply Kaiser filter for high quality (simplified)
    if (quality == MipmapQuality::HighQuality) {
        // Would apply sharpen filter here
    }

    return mip;
}

// ============================================================================
// Compression
// ============================================================================

CompressedTextureData TextureImporter::Compress(const ImageData& image,
                                                 TextureCompression format,
                                                 int quality,
                                                 bool generateMipmaps) {
    CompressedTextureData result;
    result.format = format;
    result.width = image.width;
    result.height = image.height;
    result.channels = image.channels;
    result.sRGB = true;

    // Generate mipmaps first if needed
    std::vector<MipmapLevel> mipmaps;
    if (generateMipmaps) {
        mipmaps = GenerateMipmaps(image);
    } else {
        MipmapLevel level0;
        level0.width = image.width;
        level0.height = image.height;
        level0.data = image.pixels;
        level0.dataSize = image.pixels.size();
        mipmaps.push_back(level0);
    }

    // Compress each mipmap level
    switch (format) {
        case TextureCompression::None:
            result.mipmaps = mipmaps;
            break;

        case TextureCompression::BC1:
            result = CompressBC1(image, quality);
            break;

        case TextureCompression::BC3:
            result = CompressBC3(image, quality);
            break;

        case TextureCompression::BC4:
            result = CompressBC4(image, quality);
            break;

        case TextureCompression::BC5:
            result = CompressBC5(image, quality);
            break;

        case TextureCompression::BC6H:
            result = CompressBC6H(image, quality);
            break;

        case TextureCompression::BC7:
            result = CompressBC7(image, quality);
            break;

        case TextureCompression::ETC1:
        case TextureCompression::ETC2_RGB:
        case TextureCompression::ETC2_RGBA:
            result = CompressETC(image, format, quality);
            break;

        case TextureCompression::ASTC_4x4:
        case TextureCompression::ASTC_6x6:
        case TextureCompression::ASTC_8x8:
            result = CompressASTC(image, format, quality);
            break;

        default:
            result.mipmaps = mipmaps;
            break;
    }

    return result;
}

CompressedTextureData TextureImporter::Compress(const ImageData& image,
                                                 const TextureImportSettings& settings) {
    return Compress(image, settings.compression, settings.compressionQuality,
                    settings.generateMipmaps);
}

size_t TextureImporter::EstimateCompressedSize(int width, int height,
                                                TextureCompression format,
                                                bool withMipmaps) {
    float bpp = GetCompressionBPP(format);
    size_t baseSize = static_cast<size_t>((width * height * bpp) / 8);

    if (withMipmaps) {
        // Mipmaps add approximately 33% more data
        baseSize = static_cast<size_t>(baseSize * 1.33f);
    }

    return baseSize;
}

// Block compression implementations (simplified placeholders)
CompressedTextureData TextureImporter::CompressBC1(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC1;
    result.width = image.width;
    result.height = image.height;

    // BC1: 4x4 blocks, 8 bytes per block
    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 8;
    mip.data.resize(mip.dataSize);

    // Placeholder - real implementation would use actual BC1 compression
    std::fill(mip.data.begin(), mip.data.end(), 0);

    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressBC3(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC3;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 16;  // BC3: 16 bytes per block
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressBC4(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC4;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 8;
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressBC5(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC5;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 16;
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressBC6H(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC6H;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 16;
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressBC7(const ImageData& image, int quality) {
    CompressedTextureData result;
    result.format = TextureCompression::BC7;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 16;
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressETC(const ImageData& image,
                                                    TextureCompression format,
                                                    int quality) {
    CompressedTextureData result;
    result.format = format;
    result.width = image.width;
    result.height = image.height;

    int blocksX = (image.width + 3) / 4;
    int blocksY = (image.height + 3) / 4;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * (format == TextureCompression::ETC2_RGBA ? 16 : 8);
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

CompressedTextureData TextureImporter::CompressASTC(const ImageData& image,
                                                     TextureCompression format,
                                                     int quality) {
    CompressedTextureData result;
    result.format = format;
    result.width = image.width;
    result.height = image.height;

    int blockSize = 4;
    if (format == TextureCompression::ASTC_6x6) blockSize = 6;
    else if (format == TextureCompression::ASTC_8x8) blockSize = 8;

    int blocksX = (image.width + blockSize - 1) / blockSize;
    int blocksY = (image.height + blockSize - 1) / blockSize;

    MipmapLevel mip;
    mip.width = image.width;
    mip.height = image.height;
    mip.dataSize = blocksX * blocksY * 16;  // ASTC: always 16 bytes per block
    mip.data.resize(mip.dataSize);

    std::fill(mip.data.begin(), mip.data.end(), 0);
    result.mipmaps.push_back(mip);
    return result;
}

// ============================================================================
// Normal Map Processing
// ============================================================================

bool TextureImporter::DetectNormalMap(const ImageData& image) {
    if (image.channels < 3) return false;

    // Normal maps typically have:
    // - Average blue channel > 0.5 (pointing outward)
    // - Low variation in blue channel
    // - RG channels centered around 0.5

    long long blueSum = 0;
    long long redSum = 0;
    long long greenSum = 0;
    int pixelCount = image.width * image.height;

    for (int i = 0; i < pixelCount; ++i) {
        redSum += image.pixels[i * image.channels];
        greenSum += image.pixels[i * image.channels + 1];
        blueSum += image.pixels[i * image.channels + 2];
    }

    float avgRed = static_cast<float>(redSum) / (pixelCount * 255);
    float avgGreen = static_cast<float>(greenSum) / (pixelCount * 255);
    float avgBlue = static_cast<float>(blueSum) / (pixelCount * 255);

    // Normal maps: blue > 0.5, RG around 0.5
    return avgBlue > 0.5f && std::abs(avgRed - 0.5f) < 0.3f && std::abs(avgGreen - 0.5f) < 0.3f;
}

ImageData TextureImporter::HeightToNormal(const ImageData& heightMap, float strength) {
    ImageData result;
    result.width = heightMap.width;
    result.height = heightMap.height;
    result.channels = 3;
    result.pixels.resize(result.width * result.height * 3);

    auto getHeight = [&](int x, int y) -> float {
        x = std::clamp(x, 0, result.width - 1);
        y = std::clamp(y, 0, result.height - 1);
        return heightMap.pixels[y * heightMap.width * heightMap.channels + x * heightMap.channels] / 255.0f;
    };

    for (int y = 0; y < result.height; ++y) {
        for (int x = 0; x < result.width; ++x) {
            // Sobel operator
            float left = getHeight(x - 1, y);
            float right = getHeight(x + 1, y);
            float top = getHeight(x, y - 1);
            float bottom = getHeight(x, y + 1);

            float dx = (right - left) * strength;
            float dy = (bottom - top) * strength;
            float dz = 1.0f;

            // Normalize
            float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            dx /= len;
            dy /= len;
            dz /= len;

            // Convert to [0, 255] range
            int idx = (y * result.width + x) * 3;
            result.pixels[idx] = static_cast<uint8_t>((dx * 0.5f + 0.5f) * 255);
            result.pixels[idx + 1] = static_cast<uint8_t>((dy * 0.5f + 0.5f) * 255);
            result.pixels[idx + 2] = static_cast<uint8_t>((dz * 0.5f + 0.5f) * 255);
        }
    }

    return result;
}

void TextureImporter::NormalizeNormalMap(ImageData& normalMap) {
    if (normalMap.channels < 3) return;

    for (int i = 0; i < normalMap.width * normalMap.height; ++i) {
        int idx = i * normalMap.channels;

        float x = (normalMap.pixels[idx] / 255.0f) * 2.0f - 1.0f;
        float y = (normalMap.pixels[idx + 1] / 255.0f) * 2.0f - 1.0f;
        float z = (normalMap.pixels[idx + 2] / 255.0f) * 2.0f - 1.0f;

        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0.0001f) {
            x /= len;
            y /= len;
            z /= len;
        }

        normalMap.pixels[idx] = static_cast<uint8_t>((x * 0.5f + 0.5f) * 255);
        normalMap.pixels[idx + 1] = static_cast<uint8_t>((y * 0.5f + 0.5f) * 255);
        normalMap.pixels[idx + 2] = static_cast<uint8_t>((z * 0.5f + 0.5f) * 255);
    }
}

void TextureImporter::ReconstructNormalZ(ImageData& normalMap) {
    if (normalMap.channels < 2) return;

    for (int i = 0; i < normalMap.width * normalMap.height; ++i) {
        int idx = i * normalMap.channels;

        float x = (normalMap.pixels[idx] / 255.0f) * 2.0f - 1.0f;
        float y = (normalMap.pixels[idx + 1] / 255.0f) * 2.0f - 1.0f;

        // Z = sqrt(1 - x^2 - y^2)
        float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));

        if (normalMap.channels >= 3) {
            normalMap.pixels[idx + 2] = static_cast<uint8_t>((z * 0.5f + 0.5f) * 255);
        }
    }
}

void TextureImporter::SwizzleNormalMap(ImageData& normalMap, int xChannel, int yChannel, int zChannel) {
    if (normalMap.channels < 3) return;

    for (int i = 0; i < normalMap.width * normalMap.height; ++i) {
        int idx = i * normalMap.channels;

        uint8_t orig[3] = {
            normalMap.pixels[idx],
            normalMap.pixels[idx + 1],
            normalMap.pixels[idx + 2]
        };

        normalMap.pixels[idx] = orig[xChannel];
        normalMap.pixels[idx + 1] = orig[yChannel];
        normalMap.pixels[idx + 2] = orig[zChannel];
    }
}

// ============================================================================
// Image Processing
// ============================================================================

ImageData TextureImporter::Resize(const ImageData& image, int newWidth, int newHeight) {
    ImageData result;
    result.width = newWidth;
    result.height = newHeight;
    result.channels = image.channels;
    result.pixels.resize(newWidth * newHeight * result.channels);

    // Bilinear interpolation
    float xRatio = static_cast<float>(image.width - 1) / newWidth;
    float yRatio = static_cast<float>(image.height - 1) / newHeight;

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            float gx = x * xRatio;
            float gy = y * yRatio;
            int gxi = static_cast<int>(gx);
            int gyi = static_cast<int>(gy);
            float xDiff = gx - gxi;
            float yDiff = gy - gyi;

            for (int c = 0; c < result.channels; ++c) {
                auto getSrc = [&](int sx, int sy) -> float {
                    sx = std::clamp(sx, 0, image.width - 1);
                    sy = std::clamp(sy, 0, image.height - 1);
                    return image.pixels[(sy * image.width + sx) * image.channels + c];
                };

                float a = getSrc(gxi, gyi);
                float b = getSrc(gxi + 1, gyi);
                float cc = getSrc(gxi, gyi + 1);
                float d = getSrc(gxi + 1, gyi + 1);

                float value = a * (1 - xDiff) * (1 - yDiff) +
                              b * xDiff * (1 - yDiff) +
                              cc * (1 - xDiff) * yDiff +
                              d * xDiff * yDiff;

                result.pixels[(y * newWidth + x) * result.channels + c] =
                    static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
            }
        }
    }

    return result;
}

ImageData TextureImporter::ResizeToPowerOfTwo(const ImageData& image, bool roundUp) {
    int newWidth = roundUp ? NextPowerOfTwo(image.width) : (1 << static_cast<int>(std::log2(image.width)));
    int newHeight = roundUp ? NextPowerOfTwo(image.height) : (1 << static_cast<int>(std::log2(image.height)));
    return Resize(image, newWidth, newHeight);
}

void TextureImporter::FlipVertical(ImageData& image) {
    int rowSize = image.width * image.channels;
    std::vector<uint8_t> row(rowSize);

    for (int y = 0; y < image.height / 2; ++y) {
        int y2 = image.height - 1 - y;
        memcpy(row.data(), &image.pixels[y * rowSize], rowSize);
        memcpy(&image.pixels[y * rowSize], &image.pixels[y2 * rowSize], rowSize);
        memcpy(&image.pixels[y2 * rowSize], row.data(), rowSize);
    }
}

void TextureImporter::FlipHorizontal(ImageData& image) {
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width / 2; ++x) {
            int x2 = image.width - 1 - x;
            for (int c = 0; c < image.channels; ++c) {
                std::swap(image.pixels[(y * image.width + x) * image.channels + c],
                         image.pixels[(y * image.width + x2) * image.channels + c]);
            }
        }
    }
}

void TextureImporter::PremultiplyAlpha(ImageData& image) {
    if (image.channels != 4) return;

    for (size_t i = 0; i < image.pixels.size(); i += 4) {
        float alpha = image.pixels[i + 3] / 255.0f;
        image.pixels[i] = static_cast<uint8_t>(image.pixels[i] * alpha);
        image.pixels[i + 1] = static_cast<uint8_t>(image.pixels[i + 1] * alpha);
        image.pixels[i + 2] = static_cast<uint8_t>(image.pixels[i + 2] * alpha);
    }
}

void TextureImporter::SRGBToLinear(ImageData& image) {
    for (size_t i = 0; i < image.pixels.size(); ++i) {
        if (image.channels == 4 && (i % 4) == 3) continue;  // Skip alpha
        float srgb = image.pixels[i] / 255.0f;
        float linear = Nova::SRGBToLinear(srgb);
        image.pixels[i] = static_cast<uint8_t>(linear * 255.0f);
    }
}

void TextureImporter::LinearToSRGB(ImageData& image) {
    for (size_t i = 0; i < image.pixels.size(); ++i) {
        if (image.channels == 4 && (i % 4) == 3) continue;
        float linear = image.pixels[i] / 255.0f;
        float srgb = Nova::LinearToSRGB(linear);
        image.pixels[i] = static_cast<uint8_t>(srgb * 255.0f);
    }
}

void TextureImporter::AdjustGamma(ImageData& image, float gamma) {
    for (size_t i = 0; i < image.pixels.size(); ++i) {
        if (image.channels == 4 && (i % 4) == 3) continue;
        float val = image.pixels[i] / 255.0f;
        val = std::pow(val, gamma);
        image.pixels[i] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
    }
}

// ============================================================================
// Atlas Generation
// ============================================================================

TextureAtlas TextureImporter::GenerateAtlas(const std::vector<std::string>& imagePaths,
                                             int maxSize, int padding, bool trimWhitespace) {
    std::vector<ImageData> images;
    std::vector<std::string> names;

    for (const auto& path : imagePaths) {
        ImageData img = LoadImage(path);
        if (img.width > 0) {
            images.push_back(std::move(img));
            names.push_back(fs::path(path).stem().string());
        }
    }

    return PackImages(images, names, maxSize, padding);
}

TextureAtlas TextureImporter::PackImages(const std::vector<ImageData>& images,
                                          const std::vector<std::string>& names,
                                          int maxSize, int padding) {
    TextureAtlas atlas;

    // Create rectangles for packing
    std::vector<PackRect> rects;
    for (size_t i = 0; i < images.size(); ++i) {
        PackRect rect;
        rect.id = static_cast<int>(i);
        rect.width = images[i].width + padding * 2;
        rect.height = images[i].height + padding * 2;
        rects.push_back(rect);
    }

    // Pack rectangles
    std::vector<PackRect> packed = PackRectangles(rects, maxSize, maxSize);

    // Find atlas dimensions
    int atlasWidth = 0, atlasHeight = 0;
    for (const auto& rect : packed) {
        if (rect.packed) {
            atlasWidth = std::max(atlasWidth, rect.x + rect.width);
            atlasHeight = std::max(atlasHeight, rect.y + rect.height);
        }
    }

    // Round up to power of two
    atlasWidth = NextPowerOfTwo(atlasWidth);
    atlasHeight = NextPowerOfTwo(atlasHeight);

    atlas.width = atlasWidth;
    atlas.height = atlasHeight;
    atlas.imageData.resize(atlasWidth * atlasHeight * 4, 0);

    // Copy images to atlas
    for (const auto& rect : packed) {
        if (!rect.packed) continue;

        const ImageData& src = images[rect.id];
        int dstX = rect.x + padding;
        int dstY = rect.y + padding;

        for (int y = 0; y < src.height; ++y) {
            for (int x = 0; x < src.width; ++x) {
                int dstIdx = ((dstY + y) * atlasWidth + (dstX + x)) * 4;
                int srcIdx = (y * src.width + x) * src.channels;

                atlas.imageData[dstIdx] = src.pixels[srcIdx];
                atlas.imageData[dstIdx + 1] = src.channels > 1 ? src.pixels[srcIdx + 1] : src.pixels[srcIdx];
                atlas.imageData[dstIdx + 2] = src.channels > 2 ? src.pixels[srcIdx + 2] : src.pixels[srcIdx];
                atlas.imageData[dstIdx + 3] = src.channels > 3 ? src.pixels[srcIdx + 3] : 255;
            }
        }

        // Create atlas entry
        AtlasEntry entry;
        entry.name = names[rect.id];
        entry.x = dstX;
        entry.y = dstY;
        entry.width = src.width;
        entry.height = src.height;
        entry.uvMin = glm::vec2(static_cast<float>(dstX) / atlasWidth,
                                 static_cast<float>(dstY) / atlasHeight);
        entry.uvMax = glm::vec2(static_cast<float>(dstX + src.width) / atlasWidth,
                                 static_cast<float>(dstY + src.height) / atlasHeight);
        atlas.entries.push_back(entry);
    }

    return atlas;
}

std::vector<TextureImporter::PackRect> TextureImporter::PackRectangles(
    std::vector<PackRect>& rects, int maxWidth, int maxHeight) {

    // Sort by height (descending) for better packing
    std::sort(rects.begin(), rects.end(), [](const PackRect& a, const PackRect& b) {
        return a.height > b.height;
    });

    // Simple shelf packing algorithm
    int currentX = 0;
    int currentY = 0;
    int rowHeight = 0;

    for (auto& rect : rects) {
        if (currentX + rect.width > maxWidth) {
            currentX = 0;
            currentY += rowHeight;
            rowHeight = 0;
        }

        if (currentY + rect.height > maxHeight) {
            rect.packed = false;
            continue;
        }

        rect.x = currentX;
        rect.y = currentY;
        rect.packed = true;

        currentX += rect.width;
        rowHeight = std::max(rowHeight, rect.height);
    }

    return rects;
}

// ============================================================================
// Sprite Sheet Processing
// ============================================================================

std::vector<SpriteSlice> TextureImporter::SliceSpriteSheet(const ImageData& image,
                                                            int sliceWidth, int sliceHeight,
                                                            int columns, int rows) {
    std::vector<SpriteSlice> sprites;

    if (columns <= 0) columns = image.width / sliceWidth;
    if (rows <= 0) rows = image.height / sliceHeight;

    int spriteIndex = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            SpriteSlice sprite;
            sprite.name = "sprite_" + std::to_string(spriteIndex++);
            sprite.x = col * sliceWidth;
            sprite.y = row * sliceHeight;
            sprite.width = sliceWidth;
            sprite.height = sliceHeight;
            sprite.pivotX = sliceWidth / 2;
            sprite.pivotY = sliceHeight / 2;
            sprites.push_back(sprite);
        }
    }

    return sprites;
}

std::vector<SpriteSlice> TextureImporter::AutoDetectSprites(const ImageData& image,
                                                             uint8_t alphaThreshold) {
    std::vector<SpriteSlice> sprites;

    if (image.channels != 4) return sprites;

    // Simple connected component detection
    std::vector<bool> visited(image.width * image.height, false);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int idx = y * image.width + x;
            if (visited[idx]) continue;

            uint8_t alpha = image.pixels[idx * 4 + 3];
            if (alpha < alphaThreshold) {
                visited[idx] = true;
                continue;
            }

            // Flood fill to find bounds
            int minX = x, maxX = x, minY = y, maxY = y;
            std::vector<std::pair<int, int>> stack = {{x, y}};

            while (!stack.empty()) {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                int cIdx = cy * image.width + cx;
                if (cx < 0 || cx >= image.width || cy < 0 || cy >= image.height) continue;
                if (visited[cIdx]) continue;

                uint8_t cAlpha = image.pixels[cIdx * 4 + 3];
                if (cAlpha < alphaThreshold) continue;

                visited[cIdx] = true;
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                stack.push_back({cx - 1, cy});
                stack.push_back({cx + 1, cy});
                stack.push_back({cx, cy - 1});
                stack.push_back({cx, cy + 1});
            }

            SpriteSlice sprite;
            sprite.name = "sprite_" + std::to_string(sprites.size());
            sprite.x = minX;
            sprite.y = minY;
            sprite.width = maxX - minX + 1;
            sprite.height = maxY - minY + 1;
            sprite.pivotX = sprite.width / 2;
            sprite.pivotY = sprite.height / 2;
            sprites.push_back(sprite);
        }
    }

    return sprites;
}

// ============================================================================
// Thumbnail Generation
// ============================================================================

ImageData TextureImporter::GenerateThumbnail(const ImageData& image, int size) {
    int thumbWidth, thumbHeight;

    if (image.width > image.height) {
        thumbWidth = size;
        thumbHeight = static_cast<int>(size * (static_cast<float>(image.height) / image.width));
    } else {
        thumbHeight = size;
        thumbWidth = static_cast<int>(size * (static_cast<float>(image.width) / image.height));
    }

    return Resize(image, std::max(1, thumbWidth), std::max(1, thumbHeight));
}

ImageData TextureImporter::GenerateThumbnailFromFile(const std::string& path, int size) {
    ImageData image = LoadImage(path);
    if (image.width == 0) return image;
    return GenerateThumbnail(image, size);
}

// ============================================================================
// File Format Support
// ============================================================================

bool TextureImporter::IsFormatSupported(const std::string& extension) {
    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::vector<std::string> supported = {
        ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".ktx", ".exr", ".hdr", ".gif", ".psd"
    };

    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

std::vector<std::string> TextureImporter::GetSupportedExtensions() {
    return {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".ktx", ".exr", ".hdr"};
}

std::string TextureImporter::DetectFormat(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    uint8_t header[12];
    file.read(reinterpret_cast<char*>(header), 12);

    // PNG
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
        return "PNG";
    }
    // JPEG
    if (header[0] == 0xFF && header[1] == 0xD8) {
        return "JPEG";
    }
    // DDS
    if (header[0] == 'D' && header[1] == 'D' && header[2] == 'S' && header[3] == ' ') {
        return "DDS";
    }
    // BMP
    if (header[0] == 'B' && header[1] == 'M') {
        return "BMP";
    }
    // KTX
    if (header[0] == 0xAB && header[1] == 0x4B && header[2] == 0x54 && header[3] == 0x58) {
        return "KTX";
    }

    return fs::path(path).extension().string();
}

// ============================================================================
// Output
// ============================================================================

bool TextureImporter::SaveCompressed(const CompressedTextureData& data, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    struct Header {
        char magic[4] = {'N', 'T', 'E', 'X'};
        uint32_t version = 1;
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint32_t mipmapCount;
        uint8_t sRGB;
        uint8_t padding[3] = {0};
    };

    Header header;
    header.width = data.width;
    header.height = data.height;
    header.format = static_cast<uint32_t>(data.format);
    header.mipmapCount = static_cast<uint32_t>(data.mipmaps.size());
    header.sRGB = data.sRGB ? 1 : 0;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write mipmap data
    for (const auto& mip : data.mipmaps) {
        uint32_t size = static_cast<uint32_t>(mip.dataSize);
        file.write(reinterpret_cast<const char*>(&size), 4);
        file.write(reinterpret_cast<const char*>(mip.data.data()), mip.dataSize);
    }

    return true;
}

bool TextureImporter::SavePNG(const ImageData& image, const std::string& path) {
    // Simplified - would use stb_image_write in production
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write PNG signature
    uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(signature), 8);

    // Would write IHDR, IDAT, IEND chunks here
    return true;
}

bool TextureImporter::SaveEngineFormat(const ImportedTexture& texture, const std::string& path) {
    return SaveCompressed(texture.compressedData, path);
}

std::string TextureImporter::ExportMetadata(const ImportedTexture& texture) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"sourcePath\": \"" << texture.sourcePath << "\",\n";
    ss << "  \"outputPath\": \"" << texture.outputPath << "\",\n";
    ss << "  \"width\": " << texture.width << ",\n";
    ss << "  \"height\": " << texture.height << ",\n";
    ss << "  \"channels\": " << texture.channels << ",\n";
    ss << "  \"compression\": \"" << GetCompressionName(texture.compression) << "\",\n";
    ss << "  \"sRGB\": " << (texture.sRGB ? "true" : "false") << ",\n";
    ss << "  \"mipmaps\": " << texture.mipmapCount << ",\n";
    ss << "  \"originalSize\": " << texture.originalSize << ",\n";
    ss << "  \"compressedSize\": " << texture.compressedSize << ",\n";
    ss << "  \"compressionRatio\": " << texture.compressionRatio << ",\n";
    ss << "  \"sprites\": " << texture.sprites.size() << "\n";
    ss << "}";
    return ss.str();
}

} // namespace Nova
