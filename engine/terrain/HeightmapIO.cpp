#include "terrain/HeightmapIO.hpp"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>

// stb_image is implemented in Texture.cpp - we only include the headers here
#include <stb_image.h>
#include <stb_image_write.h>

namespace Nova {

// Static member initialization
std::string HeightmapIO::s_lastError;

// ============================================================================
// HeightmapData Implementation
// ============================================================================

float HeightmapData::SampleBilinear(float u, float v) const noexcept {
    if (!IsValid()) return 0.0f;

    // Convert UV to pixel coordinates
    float px = u * (width - 1);
    float py = v * (height - 1);

    // Get integer and fractional parts
    int x0 = static_cast<int>(std::floor(px));
    int y0 = static_cast<int>(std::floor(py));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = px - x0;
    float fy = py - y0;

    // Sample four corners
    float h00 = GetHeight(x0, y0);
    float h10 = GetHeight(x1, y0);
    float h01 = GetHeight(x0, y1);
    float h11 = GetHeight(x1, y1);

    // Bilinear interpolation
    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);

    return h0 + fy * (h1 - h0);
}

std::pair<float, float> HeightmapData::CalculateMinMax() const noexcept {
    if (data.empty()) return {0.0f, 0.0f};

    float minVal = data[0];
    float maxVal = data[0];

    for (size_t i = 1; i < data.size(); ++i) {
        minVal = std::min(minVal, data[i]);
        maxVal = std::max(maxVal, data[i]);
    }

    return {minVal, maxVal};
}

void HeightmapData::Normalize() {
    auto [minVal, maxVal] = CalculateMinMax();

    if (maxVal - minVal < 1e-6f) {
        // All values are the same, set to 0
        std::fill(data.begin(), data.end(), 0.0f);
        return;
    }

    float range = maxVal - minVal;
    for (float& h : data) {
        h = (h - minVal) / range;
    }
}

// ============================================================================
// HeightmapIO - Error Handling
// ============================================================================

void HeightmapIO::SetError(const std::string& error) {
    s_lastError = error;
    spdlog::error("HeightmapIO: {}", error);
}

// ============================================================================
// HeightmapIO - Conversion Helpers
// ============================================================================

float HeightmapIO::ByteToFloat(uint8_t value) noexcept {
    return static_cast<float>(value) / 255.0f;
}

float HeightmapIO::UInt16ToFloat(uint16_t value) noexcept {
    return static_cast<float>(value) / 65535.0f;
}

uint8_t HeightmapIO::FloatToByte(float value) noexcept {
    value = glm::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

uint16_t HeightmapIO::FloatToUInt16(float value) noexcept {
    value = glm::clamp(value, 0.0f, 1.0f);
    return static_cast<uint16_t>(value * 65535.0f + 0.5f);
}

// ============================================================================
// HeightmapIO - File Utilities
// ============================================================================

std::string HeightmapIO::GetFileExtension(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return ext;
}

bool HeightmapIO::FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

// ============================================================================
// HeightmapIO - Import Functions
// ============================================================================

HeightmapResult HeightmapIO::LoadFromPNG(
    const std::string& path,
    const HeightmapImportOptions& options)
{
    if (!FileExists(path)) {
        return HeightmapResult::Error("File not found: " + path);
    }

    // Try loading as 16-bit first
    int width, height, channels;
    stbi_us* data16 = stbi_load_16(path.c_str(), &width, &height, &channels, 0);

    HeightmapData heightmap;

    if (data16) {
        // Successfully loaded as 16-bit
        heightmap.Allocate(width, height);

        for (int y = 0; y < height; ++y) {
            int srcY = options.invertY ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                int srcIdx = srcY * width * channels + x * channels;
                float h;

                if (channels == 1) {
                    // Grayscale
                    h = UInt16ToFloat(data16[srcIdx]);
                } else {
                    // RGB(A) - convert to grayscale using luminance
                    float r = UInt16ToFloat(data16[srcIdx]);
                    float g = UInt16ToFloat(data16[srcIdx + 1]);
                    float b = UInt16ToFloat(data16[srcIdx + 2]);
                    h = 0.299f * r + 0.587f * g + 0.114f * b;
                }

                heightmap.SetHeight(x, y, h);
            }
        }

        stbi_image_free(data16);
        spdlog::info("Loaded 16-bit PNG heightmap: {} ({}x{})", path, width, height);
    } else {
        // Try loading as 8-bit
        stbi_uc* data8 = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data8) {
            return HeightmapResult::Error("Failed to load PNG: " + std::string(stbi_failure_reason()));
        }

        heightmap.Allocate(width, height);

        for (int y = 0; y < height; ++y) {
            int srcY = options.invertY ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                int srcIdx = srcY * width * channels + x * channels;
                float h;

                if (channels == 1) {
                    // Grayscale
                    h = ByteToFloat(data8[srcIdx]);
                } else {
                    // RGB(A) - convert to grayscale using luminance
                    float r = ByteToFloat(data8[srcIdx]);
                    float g = ByteToFloat(data8[srcIdx + 1]);
                    float b = ByteToFloat(data8[srcIdx + 2]);
                    h = 0.299f * r + 0.587f * g + 0.114f * b;
                }

                heightmap.SetHeight(x, y, h);
            }
        }

        stbi_image_free(data8);
        spdlog::info("Loaded 8-bit PNG heightmap: {} ({}x{})", path, width, height);
    }

    // Apply height inversion if requested
    if (options.invertHeight) {
        for (float& h : heightmap.data) {
            h = 1.0f - h;
        }
    }

    // Apply source height range mapping
    if (options.sourceMaxHeight != options.sourceMinHeight) {
        float srcRange = options.sourceMaxHeight - options.sourceMinHeight;
        for (float& h : heightmap.data) {
            // Map from normalized to source range, then back to normalized
            h = (h - options.sourceMinHeight / srcRange);
            h = glm::clamp(h, 0.0f, 1.0f);
        }
    }

    // Normalize if requested
    if (options.normalizeHeight) {
        heightmap.Normalize();
    }

    // Apply height scale
    if (options.heightScale != 1.0f) {
        for (float& h : heightmap.data) {
            h *= options.heightScale;
        }
    }

    // Set world height range
    heightmap.minHeight = options.targetMinHeight;
    heightmap.maxHeight = options.targetMaxHeight;

    return HeightmapResult::Success(std::move(heightmap));
}

HeightmapResult HeightmapIO::LoadFromRAW(
    const std::string& path,
    int width,
    int height,
    const HeightmapImportOptions& options)
{
    if (width <= 0) {
        return HeightmapResult::Error("Invalid width for RAW file");
    }

    if (height <= 0) {
        height = width;  // Assume square if height not specified
    }

    if (!FileExists(path)) {
        return HeightmapResult::Error("File not found: " + path);
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return HeightmapResult::Error("Failed to open file: " + path);
    }

    // Check file size
    std::streamsize fileSize = file.tellg();
    std::streamsize expectedSize = static_cast<std::streamsize>(width) * height * 2;  // 16-bit = 2 bytes

    if (fileSize != expectedSize) {
        return HeightmapResult::Error(
            "File size mismatch. Expected " + std::to_string(expectedSize) +
            " bytes for " + std::to_string(width) + "x" + std::to_string(height) +
            " RAW, got " + std::to_string(fileSize) + " bytes");
    }

    file.seekg(0, std::ios::beg);

    // Read raw data
    std::vector<uint16_t> rawData(static_cast<size_t>(width) * height);
    file.read(reinterpret_cast<char*>(rawData.data()), expectedSize);

    if (!file) {
        return HeightmapResult::Error("Failed to read RAW data");
    }

    file.close();

    HeightmapData heightmap;
    heightmap.Allocate(width, height);

    for (int y = 0; y < height; ++y) {
        int srcY = options.invertY ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            int srcIdx = srcY * width + x;
            uint16_t rawValue = rawData[srcIdx];

            // Handle byte order
            if (!options.littleEndian) {
                rawValue = ((rawValue & 0xFF) << 8) | ((rawValue >> 8) & 0xFF);
            }

            float h;
            if (options.signedFormat) {
                // Signed 16-bit: range [-32768, 32767] -> [0, 1]
                int16_t signedValue = static_cast<int16_t>(rawValue);
                h = (static_cast<float>(signedValue) + 32768.0f) / 65535.0f;
            } else {
                // Unsigned 16-bit: range [0, 65535] -> [0, 1]
                h = UInt16ToFloat(rawValue);
            }

            heightmap.SetHeight(x, y, h);
        }
    }

    spdlog::info("Loaded RAW heightmap: {} ({}x{})", path, width, height);

    // Apply height inversion if requested
    if (options.invertHeight) {
        for (float& h : heightmap.data) {
            h = 1.0f - h;
        }
    }

    // Normalize if requested
    if (options.normalizeHeight) {
        heightmap.Normalize();
    }

    // Apply height scale
    if (options.heightScale != 1.0f) {
        for (float& h : heightmap.data) {
            h *= options.heightScale;
        }
    }

    // Set world height range
    heightmap.minHeight = options.targetMinHeight;
    heightmap.maxHeight = options.targetMaxHeight;

    return HeightmapResult::Success(std::move(heightmap));
}

HeightmapResult HeightmapIO::Load(
    const std::string& path,
    const HeightmapImportOptions& options,
    int rawWidth,
    int rawHeight)
{
    std::string ext = GetFileExtension(path);

    if (ext == ".png") {
        return LoadFromPNG(path, options);
    } else if (ext == ".raw" || ext == ".r16") {
        if (rawWidth <= 0) {
            return HeightmapResult::Error("Width must be specified for RAW files");
        }
        return LoadFromRAW(path, rawWidth, rawHeight, options);
    } else {
        return HeightmapResult::Error("Unsupported file format: " + ext);
    }
}

// ============================================================================
// HeightmapIO - Export Functions
// ============================================================================

bool HeightmapIO::SaveToPNG(
    const HeightmapData& heightmap,
    const std::string& path,
    int bits,
    const HeightmapExportOptions& options)
{
    if (!heightmap.IsValid()) {
        SetError("Invalid heightmap data");
        return false;
    }

    if (bits != 8 && bits != 16) {
        SetError("PNG bit depth must be 8 or 16");
        return false;
    }

    int width = heightmap.width;
    int height = heightmap.height;

    // Find min/max for normalization if requested
    float minVal = 0.0f;
    float maxVal = 1.0f;
    if (options.normalize) {
        auto [minH, maxH] = heightmap.CalculateMinMax();
        minVal = minH;
        maxVal = maxH;
    }
    float range = maxVal - minVal;
    if (range < 1e-6f) range = 1.0f;

    int result;

    if (bits == 16) {
        // 16-bit PNG output
        std::vector<uint16_t> outData(static_cast<size_t>(width) * height);

        for (int y = 0; y < height; ++y) {
            int dstY = options.invertY ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                float h = heightmap.GetHeight(x, y);

                // Normalize to [0, 1]
                if (options.normalize) {
                    h = (h - minVal) / range;
                }

                outData[dstY * width + x] = FloatToUInt16(h);
            }
        }

        // stbi_write_png writes in network byte order (big endian) for 16-bit
        // We need to swap bytes on little-endian systems
        for (uint16_t& val : outData) {
            val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
        }

        result = stbi_write_png(path.c_str(), width, height, 1,
                                outData.data(), width * sizeof(uint16_t));
    } else {
        // 8-bit PNG output
        std::vector<uint8_t> outData(static_cast<size_t>(width) * height);

        for (int y = 0; y < height; ++y) {
            int dstY = options.invertY ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                float h = heightmap.GetHeight(x, y);

                // Normalize to [0, 1]
                if (options.normalize) {
                    h = (h - minVal) / range;
                }

                outData[dstY * width + x] = FloatToByte(h);
            }
        }

        result = stbi_write_png(path.c_str(), width, height, 1,
                                outData.data(), width);
    }

    if (!result) {
        SetError("Failed to write PNG file: " + path);
        return false;
    }

    spdlog::info("Saved {}bit PNG heightmap: {} ({}x{})", bits, path, width, height);
    return true;
}

bool HeightmapIO::SaveToRAW(
    const HeightmapData& heightmap,
    const std::string& path,
    const HeightmapExportOptions& options)
{
    if (!heightmap.IsValid()) {
        SetError("Invalid heightmap data");
        return false;
    }

    int width = heightmap.width;
    int height = heightmap.height;

    // Find min/max for normalization if requested
    float minVal = 0.0f;
    float maxVal = 1.0f;
    if (options.normalize) {
        auto [minH, maxH] = heightmap.CalculateMinMax();
        minVal = minH;
        maxVal = maxH;
    }
    float range = maxVal - minVal;
    if (range < 1e-6f) range = 1.0f;

    std::vector<uint16_t> outData(static_cast<size_t>(width) * height);

    for (int y = 0; y < height; ++y) {
        int dstY = options.invertY ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            float h = heightmap.GetHeight(x, y);

            // Normalize to [0, 1]
            if (options.normalize) {
                h = (h - minVal) / range;
            }

            uint16_t val = FloatToUInt16(h);

            // Handle byte order
            if (!options.littleEndian) {
                val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
            }

            outData[dstY * width + x] = val;
        }
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        SetError("Failed to open file for writing: " + path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(outData.data()),
               static_cast<std::streamsize>(outData.size()) * sizeof(uint16_t));

    if (!file) {
        SetError("Failed to write RAW data");
        return false;
    }

    file.close();

    spdlog::info("Saved RAW heightmap: {} ({}x{})", path, width, height);
    return true;
}

bool HeightmapIO::Save(
    const HeightmapData& heightmap,
    const std::string& path,
    const HeightmapExportOptions& options)
{
    std::string ext = GetFileExtension(path);

    if (ext == ".png") {
        return SaveToPNG(heightmap, path, 16, options);
    } else if (ext == ".raw" || ext == ".r16") {
        return SaveToRAW(heightmap, path, options);
    } else {
        SetError("Unsupported file format: " + ext);
        return false;
    }
}

// ============================================================================
// HeightmapIO - Utility Functions
// ============================================================================

HeightmapData HeightmapIO::CreateEmpty(int width, int height, float initialHeight) {
    HeightmapData heightmap;
    heightmap.Allocate(width, height, initialHeight);
    return heightmap;
}

HeightmapData HeightmapIO::CreateFromFloat(
    const float* data,
    int width,
    int height,
    bool normalize)
{
    HeightmapData heightmap;
    heightmap.width = width;
    heightmap.height = height;
    heightmap.data.assign(data, data + static_cast<size_t>(width) * height);

    if (normalize) {
        heightmap.Normalize();
    }

    return heightmap;
}

HeightmapData HeightmapIO::Resample(
    const HeightmapData& heightmap,
    int newWidth,
    int newHeight)
{
    if (!heightmap.IsValid() || newWidth <= 0 || newHeight <= 0) {
        return {};
    }

    HeightmapData result;
    result.Allocate(newWidth, newHeight);
    result.minHeight = heightmap.minHeight;
    result.maxHeight = heightmap.maxHeight;

    for (int y = 0; y < newHeight; ++y) {
        float v = static_cast<float>(y) / (newHeight - 1);
        for (int x = 0; x < newWidth; ++x) {
            float u = static_cast<float>(x) / (newWidth - 1);
            result.SetHeight(x, y, heightmap.SampleBilinear(u, v));
        }
    }

    return result;
}

HeightmapData HeightmapIO::GaussianBlur(
    const HeightmapData& heightmap,
    int radius)
{
    if (!heightmap.IsValid() || radius <= 0) {
        return heightmap;
    }

    // Generate Gaussian kernel
    int kernelSize = radius * 2 + 1;
    std::vector<float> kernel(kernelSize);
    float sigma = radius / 3.0f;
    float sum = 0.0f;

    for (int i = 0; i < kernelSize; ++i) {
        float x = static_cast<float>(i - radius);
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }

    // Normalize kernel
    for (float& k : kernel) {
        k /= sum;
    }

    int width = heightmap.width;
    int height = heightmap.height;

    // Horizontal pass
    HeightmapData temp;
    temp.Allocate(width, height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float h = 0.0f;
            for (int i = 0; i < kernelSize; ++i) {
                int sx = glm::clamp(x + i - radius, 0, width - 1);
                h += heightmap.GetHeight(sx, y) * kernel[i];
            }
            temp.SetHeight(x, y, h);
        }
    }

    // Vertical pass
    HeightmapData result;
    result.Allocate(width, height);
    result.minHeight = heightmap.minHeight;
    result.maxHeight = heightmap.maxHeight;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float h = 0.0f;
            for (int i = 0; i < kernelSize; ++i) {
                int sy = glm::clamp(y + i - radius, 0, height - 1);
                h += temp.GetHeight(x, sy) * kernel[i];
            }
            result.SetHeight(x, y, h);
        }
    }

    return result;
}

std::vector<uint8_t> HeightmapIO::GenerateNormalMap(
    const HeightmapData& heightmap,
    float strength)
{
    if (!heightmap.IsValid()) {
        return {};
    }

    int width = heightmap.width;
    int height = heightmap.height;
    std::vector<uint8_t> normalMap(static_cast<size_t>(width) * height * 4);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Sample neighboring heights using Sobel filter
            float tl = heightmap.GetHeight(x - 1, y - 1);
            float t  = heightmap.GetHeight(x,     y - 1);
            float tr = heightmap.GetHeight(x + 1, y - 1);
            float l  = heightmap.GetHeight(x - 1, y);
            float r  = heightmap.GetHeight(x + 1, y);
            float bl = heightmap.GetHeight(x - 1, y + 1);
            float b  = heightmap.GetHeight(x,     y + 1);
            float br = heightmap.GetHeight(x + 1, y + 1);

            // Sobel filter for derivatives
            float dx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
            float dy = (bl + 2.0f * b + br) - (tl + 2.0f * t + tr);

            // Create normal vector
            glm::vec3 normal(-dx * strength, -dy * strength, 1.0f);
            normal = glm::normalize(normal);

            // Convert to [0, 255] range
            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            normalMap[idx + 0] = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
            normalMap[idx + 1] = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
            normalMap[idx + 2] = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
            normalMap[idx + 3] = 255;  // Alpha
        }
    }

    return normalMap;
}

} // namespace Nova
