#include "PlaceholderGenerator.hpp"

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace Vehement {

// =============================================================================
// Permutation table for Perlin noise
// =============================================================================
const std::array<int, 512> PlaceholderGenerator::s_permutation = []() {
    std::array<int, 512> p;
    // Classic Perlin permutation table
    constexpr std::array<int, 256> base = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
        8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
        35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
        134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
        55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
        18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
        250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
        189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
        172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
        228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
        107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    for (int i = 0; i < 256; ++i) {
        p[i] = base[i];
        p[i + 256] = base[i];
    }
    return p;
}();

// =============================================================================
// ImageBuffer Implementation
// =============================================================================

ImageBuffer::ImageBuffer(int width, int height)
    : m_width(width), m_height(height), m_data(width * height) {
    Clear();
}

void ImageBuffer::SetPixel(int x, int y, const Pixel& pixel) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_data[y * m_width + x] = pixel;
    }
}

Pixel ImageBuffer::GetPixel(int x, int y) const {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_data[y * m_width + x];
    }
    return Pixel();
}

void ImageBuffer::Fill(const Pixel& pixel) {
    std::fill(m_data.begin(), m_data.end(), pixel);
}

void ImageBuffer::Clear() {
    Fill(Pixel(0, 0, 0, 255));
}

std::vector<uint8_t> ImageBuffer::GetRawData() const {
    std::vector<uint8_t> raw;
    raw.reserve(m_data.size() * 4);
    for (const auto& pixel : m_data) {
        raw.push_back(pixel.r);
        raw.push_back(pixel.g);
        raw.push_back(pixel.b);
        raw.push_back(pixel.a);
    }
    return raw;
}

// Simple PNG writer (uncompressed)
bool ImageBuffer::SavePNG(const std::string& path) const {
    // Create parent directories if needed
    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }

    // PNG signature
    const uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(signature), 8);

    auto writeUint32BE = [&](uint32_t value) {
        uint8_t bytes[4] = {
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
        file.write(reinterpret_cast<const char*>(bytes), 4);
    };

    // CRC32 calculation
    auto crc32 = [](const std::vector<uint8_t>& data) -> uint32_t {
        static uint32_t table[256];
        static bool initialized = false;
        if (!initialized) {
            for (uint32_t n = 0; n < 256; n++) {
                uint32_t c = n;
                for (int k = 0; k < 8; k++) {
                    if (c & 1)
                        c = 0xEDB88320 ^ (c >> 1);
                    else
                        c >>= 1;
                }
                table[n] = c;
            }
            initialized = true;
        }
        uint32_t crc = 0xFFFFFFFF;
        for (uint8_t byte : data) {
            crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF;
    };

    auto writeChunk = [&](const char* type, const std::vector<uint8_t>& data) {
        writeUint32BE(static_cast<uint32_t>(data.size()));
        file.write(type, 4);
        if (!data.empty()) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        std::vector<uint8_t> crcData;
        crcData.insert(crcData.end(), type, type + 4);
        crcData.insert(crcData.end(), data.begin(), data.end());
        writeUint32BE(crc32(crcData));
    };

    // IHDR chunk
    std::vector<uint8_t> ihdr;
    ihdr.resize(13);
    ihdr[0] = (m_width >> 24) & 0xFF;
    ihdr[1] = (m_width >> 16) & 0xFF;
    ihdr[2] = (m_width >> 8) & 0xFF;
    ihdr[3] = m_width & 0xFF;
    ihdr[4] = (m_height >> 24) & 0xFF;
    ihdr[5] = (m_height >> 16) & 0xFF;
    ihdr[6] = (m_height >> 8) & 0xFF;
    ihdr[7] = m_height & 0xFF;
    ihdr[8] = 8;  // Bit depth
    ihdr[9] = 6;  // Color type (RGBA)
    ihdr[10] = 0; // Compression method
    ihdr[11] = 0; // Filter method
    ihdr[12] = 0; // Interlace method
    writeChunk("IHDR", ihdr);

    // IDAT chunk (uncompressed using zlib store)
    std::vector<uint8_t> rawImageData;
    for (int y = 0; y < m_height; ++y) {
        rawImageData.push_back(0); // Filter type: None
        for (int x = 0; x < m_width; ++x) {
            const Pixel& p = m_data[y * m_width + x];
            rawImageData.push_back(p.r);
            rawImageData.push_back(p.g);
            rawImageData.push_back(p.b);
            rawImageData.push_back(p.a);
        }
    }

    // Create uncompressed zlib stream
    std::vector<uint8_t> idat;
    idat.push_back(0x78); // CMF
    idat.push_back(0x01); // FLG

    // Split into blocks of max 65535 bytes
    size_t pos = 0;
    while (pos < rawImageData.size()) {
        size_t remaining = rawImageData.size() - pos;
        size_t blockSize = std::min(remaining, size_t(65535));
        bool lastBlock = (pos + blockSize >= rawImageData.size());

        idat.push_back(lastBlock ? 0x01 : 0x00); // BFINAL + BTYPE
        idat.push_back(blockSize & 0xFF);
        idat.push_back((blockSize >> 8) & 0xFF);
        idat.push_back(~blockSize & 0xFF);
        idat.push_back((~blockSize >> 8) & 0xFF);

        for (size_t i = 0; i < blockSize; ++i) {
            idat.push_back(rawImageData[pos + i]);
        }
        pos += blockSize;
    }

    // Adler32 checksum
    uint32_t a = 1, b = 0;
    for (uint8_t byte : rawImageData) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    uint32_t adler = (b << 16) | a;
    idat.push_back((adler >> 24) & 0xFF);
    idat.push_back((adler >> 16) & 0xFF);
    idat.push_back((adler >> 8) & 0xFF);
    idat.push_back(adler & 0xFF);

    writeChunk("IDAT", idat);

    // IEND chunk
    writeChunk("IEND", {});

    return true;
}

// =============================================================================
// Noise Generation Helpers
// =============================================================================

float PlaceholderGenerator::Fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PlaceholderGenerator::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float PlaceholderGenerator::Grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float PlaceholderGenerator::PerlinNoise2D(float x, float y) {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);

    float u = Fade(x);
    float v = Fade(y);

    int A = s_permutation[X] + Y;
    int B = s_permutation[X + 1] + Y;

    return Lerp(
        Lerp(Grad(s_permutation[A], x, y), Grad(s_permutation[B], x - 1, y), u),
        Lerp(Grad(s_permutation[A + 1], x, y - 1), Grad(s_permutation[B + 1], x - 1, y - 1), u),
        v
    );
}

float PlaceholderGenerator::FractalNoise2D(float x, float y, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += PerlinNoise2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

uint32_t PlaceholderGenerator::Hash(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

float PlaceholderGenerator::RandomFloat(int x, int y) {
    uint32_t h = Hash(static_cast<uint32_t>(x) * 374761393 + static_cast<uint32_t>(y) * 668265263);
    return static_cast<float>(h) / static_cast<float>(UINT32_MAX);
}

// =============================================================================
// Drawing Helpers
// =============================================================================

void PlaceholderGenerator::DrawLine(ImageBuffer& image, int x0, int y0, int x1, int y1, const Pixel& color) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        image.SetPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void PlaceholderGenerator::DrawCircle(ImageBuffer& image, int cx, int cy, int radius, const Pixel& color, bool filled) {
    if (filled) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius) {
                    image.SetPixel(cx + x, cy + y, color);
                }
            }
        }
    } else {
        int x = radius;
        int y = 0;
        int err = 0;
        while (x >= y) {
            image.SetPixel(cx + x, cy + y, color);
            image.SetPixel(cx + y, cy + x, color);
            image.SetPixel(cx - y, cy + x, color);
            image.SetPixel(cx - x, cy + y, color);
            image.SetPixel(cx - x, cy - y, color);
            image.SetPixel(cx - y, cy - x, color);
            image.SetPixel(cx + y, cy - x, color);
            image.SetPixel(cx + x, cy - y, color);
            y++;
            if (err <= 0) {
                err += 2 * y + 1;
            } else {
                x--;
                err += 2 * (y - x) + 1;
            }
        }
    }
}

void PlaceholderGenerator::DrawRect(ImageBuffer& image, int x, int y, int w, int h, const Pixel& color, bool filled) {
    if (filled) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                image.SetPixel(px, py, color);
            }
        }
    } else {
        for (int px = x; px < x + w; ++px) {
            image.SetPixel(px, y, color);
            image.SetPixel(px, y + h - 1, color);
        }
        for (int py = y; py < y + h; ++py) {
            image.SetPixel(x, py, color);
            image.SetPixel(x + w - 1, py, color);
        }
    }
}

void PlaceholderGenerator::ApplyNoiseToImage(ImageBuffer& image, const glm::vec3& baseColor, float variation, float scale) {
    int w = image.GetWidth();
    int h = image.GetHeight();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float nx = static_cast<float>(x) / w * scale;
            float ny = static_cast<float>(y) / h * scale;
            float noise = FractalNoise2D(nx, ny, 4, 0.5f);
            noise = (noise + 1.0f) * 0.5f; // Normalize to 0-1

            glm::vec3 color = baseColor + glm::vec3(noise - 0.5f) * variation * 2.0f;
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }
}

// =============================================================================
// Texture Generation
// =============================================================================

void PlaceholderGenerator::GenerateNoiseTexture(
    const std::string& path,
    const glm::vec3& baseColor,
    int size,
    float variation,
    float scale) {

    ImageBuffer image(size, size);
    ApplyNoiseToImage(image, baseColor, variation, scale);
    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateNormalMap(const std::string& path, int size, float bumpiness) {
    ImageBuffer image(size, size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float nx = 0.0f;
            float ny = 0.0f;
            float nz = 1.0f;

            if (bumpiness > 0.0f) {
                float scale = 4.0f;
                float fx = static_cast<float>(x) / size * scale;
                float fy = static_cast<float>(y) / size * scale;

                // Calculate gradient from noise
                float h = FractalNoise2D(fx, fy, 3, 0.5f);
                float hx = FractalNoise2D(fx + 0.01f, fy, 3, 0.5f);
                float hy = FractalNoise2D(fx, fy + 0.01f, 3, 0.5f);

                nx = (h - hx) * bumpiness * 10.0f;
                ny = (h - hy) * bumpiness * 10.0f;
                nz = 1.0f;

                // Normalize
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                nx /= len;
                ny /= len;
                nz /= len;
            }

            // Convert from [-1,1] to [0,1] range
            uint8_t r = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255);
            uint8_t g = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255);
            uint8_t b = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255);

            image.SetPixel(x, y, Pixel(r, g, b, 255));
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateCheckerTexture(
    const std::string& path,
    const glm::vec3& color1,
    const glm::vec3& color2,
    int size,
    int checkerSize) {

    ImageBuffer image(size, size);
    Pixel p1 = Pixel::FromVec3(color1);
    Pixel p2 = Pixel::FromVec3(color2);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool isChecker1 = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            image.SetPixel(x, y, isChecker1 ? p1 : p2);
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateBrickTexture(
    const std::string& path,
    const glm::vec3& brickColor,
    const glm::vec3& mortarColor,
    int size,
    int brickWidth,
    int brickHeight,
    int mortarWidth) {

    ImageBuffer image(size, size);
    Pixel mortar = Pixel::FromVec3(mortarColor);

    // Fill with mortar
    image.Fill(mortar);

    // Draw bricks
    for (int y = 0; y < size; y += brickHeight + mortarWidth) {
        int rowOffset = ((y / (brickHeight + mortarWidth)) % 2) * (brickWidth / 2);
        for (int x = -brickWidth; x < size + brickWidth; x += brickWidth + mortarWidth) {
            int bx = x + rowOffset;

            // Vary brick color slightly
            float noiseVal = RandomFloat(bx, y);
            glm::vec3 variedColor = brickColor * (0.85f + noiseVal * 0.3f);
            variedColor = glm::clamp(variedColor, glm::vec3(0.0f), glm::vec3(1.0f));
            Pixel brick = Pixel::FromVec3(variedColor);

            DrawRect(image, bx, y, brickWidth, brickHeight, brick, true);
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateWoodTexture(
    const std::string& path,
    const glm::vec3& baseColor,
    int size,
    float grainDensity) {

    ImageBuffer image(size, size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Create wood grain pattern
            float grain = std::sin(fy * grainDensity + PerlinNoise2D(fx * 2.0f, fy * 0.5f) * 2.0f);
            grain = (grain + 1.0f) * 0.5f;

            // Add some variation
            float noise = FractalNoise2D(fx * 4.0f, fy * 4.0f, 3, 0.5f);
            noise = (noise + 1.0f) * 0.5f;

            glm::vec3 darkColor = baseColor * 0.6f;
            glm::vec3 color = glm::mix(darkColor, baseColor, grain);
            color += glm::vec3(noise - 0.5f) * 0.1f;
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateMetalTexture(
    const std::string& path,
    const glm::vec3& baseColor,
    int size) {

    ImageBuffer image(size, size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Create brushed metal streaks
            float streaks = PerlinNoise2D(fx * 0.5f, fy * 20.0f);
            streaks = (streaks + 1.0f) * 0.5f;

            // Add specular variation
            float spec = FractalNoise2D(fx * 8.0f, fy * 8.0f, 2, 0.5f);
            spec = (spec + 1.0f) * 0.5f;

            glm::vec3 color = baseColor * (0.8f + streaks * 0.2f + spec * 0.1f);
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateThatchTexture(
    const std::string& path,
    const glm::vec3& baseColor,
    int size) {

    ImageBuffer image(size, size);

    // Base color fill
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float noise = FractalNoise2D(x * 0.05f, y * 0.05f, 3, 0.5f);
            noise = (noise + 1.0f) * 0.5f;
            glm::vec3 color = baseColor * (0.7f + noise * 0.4f);
            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }

    // Draw straw lines
    for (int i = 0; i < size * 2; ++i) {
        int x0 = static_cast<int>(RandomFloat(i, 0) * size);
        int y0 = static_cast<int>(RandomFloat(i, 1) * size);
        int len = 10 + static_cast<int>(RandomFloat(i, 2) * 30);
        float angle = RandomFloat(i, 3) * 0.4f - 0.2f; // Slight angle variation

        int x1 = x0 + static_cast<int>(std::cos(angle) * len);
        int y1 = y0 + static_cast<int>(std::sin(angle + 1.5f) * len);

        float bright = 0.8f + RandomFloat(i, 4) * 0.4f;
        glm::vec3 strawColor = baseColor * bright;
        DrawLine(image, x0, y0, x1, y1, Pixel::FromVec3(strawColor));
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateWaterTexture(
    const std::string& path,
    const glm::vec3& baseColor,
    int size,
    float waveIntensity) {

    ImageBuffer image(size, size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Multiple wave layers
            float wave1 = std::sin(fx * 10.0f + fy * 5.0f) * 0.5f + 0.5f;
            float wave2 = std::sin(fx * 15.0f - fy * 8.0f) * 0.5f + 0.5f;
            float noise = FractalNoise2D(fx * 4.0f, fy * 4.0f, 3, 0.5f);
            noise = (noise + 1.0f) * 0.5f;

            float combined = (wave1 + wave2 + noise) / 3.0f;
            combined = combined * waveIntensity + (1.0f - waveIntensity) * 0.5f;

            glm::vec3 darkWater = baseColor * 0.7f;
            glm::vec3 lightWater = baseColor * 1.2f;
            glm::vec3 color = glm::mix(darkWater, lightWater, combined);
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateRoadTexture(
    const std::string& path,
    const glm::vec3& roadColor,
    const glm::vec3& markingColor,
    int size) {

    ImageBuffer image(size, size);

    // Fill with road color (with noise)
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float noise = FractalNoise2D(x * 0.1f, y * 0.1f, 3, 0.5f);
            noise = (noise + 1.0f) * 0.5f;
            glm::vec3 color = roadColor * (0.9f + noise * 0.2f);
            image.SetPixel(x, y, Pixel::FromVec3(color));
        }
    }

    // Draw center marking
    int centerX = size / 2;
    int markingWidth = size / 32;
    Pixel marking = Pixel::FromVec3(markingColor);

    for (int y = 0; y < size; y += size / 8) {
        int dashLength = size / 16;
        DrawRect(image, centerX - markingWidth / 2, y, markingWidth, dashLength, marking, true);
    }

    // Draw edge lines
    DrawRect(image, size / 16, 0, markingWidth / 2, size, marking, true);
    DrawRect(image, size - size / 16 - markingWidth / 2, 0, markingWidth / 2, size, marking, true);

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateIcon(
    const std::string& path,
    const std::string& iconType,
    const glm::vec3& primaryColor,
    int size) {

    ImageBuffer image(size, size);
    image.Fill(Pixel(0, 0, 0, 0)); // Transparent background

    int cx = size / 2;
    int cy = size / 2;
    int r = size / 3;
    Pixel color = Pixel::FromVec3(primaryColor);
    Pixel outline = Pixel::FromVec3(primaryColor * 0.6f);

    if (iconType == "food") {
        // Apple shape
        DrawCircle(image, cx, cy + r / 4, r, color, true);
        DrawRect(image, cx - 2, cy - r, 4, r / 2, Pixel::FromVec3(glm::vec3(0.4f, 0.2f, 0.1f)), true);
        DrawCircle(image, cx + r / 3, cy - r / 3, r / 4, Pixel::FromVec3(glm::vec3(0.2f, 0.6f, 0.2f)), true);
    } else if (iconType == "wood") {
        // Log shape
        DrawCircle(image, cx, cy, r, color, true);
        DrawCircle(image, cx, cy, r * 2 / 3, Pixel::FromVec3(primaryColor * 0.8f), true);
        DrawCircle(image, cx, cy, r / 3, Pixel::FromVec3(primaryColor * 0.6f), true);
        // Growth rings (circles)
        DrawCircle(image, cx, cy, r / 2, Pixel::FromVec3(primaryColor * 0.7f), false);
    } else if (iconType == "stone") {
        // Rock shape (irregular polygon approximated by overlapping circles)
        DrawCircle(image, cx, cy, r, color, true);
        DrawCircle(image, cx - r / 3, cy - r / 4, r * 2 / 3, Pixel::FromVec3(primaryColor * 0.9f), true);
        DrawCircle(image, cx + r / 4, cy + r / 3, r / 2, Pixel::FromVec3(primaryColor * 0.85f), true);
    } else if (iconType == "metal") {
        // Ingot shape
        int w = r * 3 / 2;
        int h = r;
        DrawRect(image, cx - w / 2, cy - h / 2, w, h, color, true);
        // Shine
        DrawRect(image, cx - w / 2 + 4, cy - h / 2 + 4, w / 3, 4, Pixel::FromVec3(glm::vec3(1.0f)), true);
    } else if (iconType == "coins") {
        // Stacked coins
        for (int i = 2; i >= 0; --i) {
            int offsetY = i * 6 - 6;
            DrawCircle(image, cx, cy + offsetY, r - 2, outline, true);
            DrawCircle(image, cx, cy + offsetY - 2, r - 4, color, true);
        }
    } else {
        // Default circle icon
        DrawCircle(image, cx, cy, r, color, true);
    }

    image.SavePNG(path);
}

void PlaceholderGenerator::GenerateBarTexture(
    const std::string& path,
    const glm::vec3& barColor,
    const glm::vec3& backgroundColor,
    int width,
    int height) {

    ImageBuffer image(width, height);

    // Background
    Pixel bg = Pixel::FromVec3(backgroundColor);
    image.Fill(bg);

    // Border
    Pixel border = Pixel::FromVec3(backgroundColor * 0.5f);
    DrawRect(image, 0, 0, width, height, border, false);

    // Gradient fill
    for (int x = 2; x < width - 2; ++x) {
        float t = static_cast<float>(x) / width;
        glm::vec3 color = barColor * (0.8f + t * 0.2f);
        for (int y = 2; y < height - 2; ++y) {
            float yGrad = 1.0f - std::abs(static_cast<float>(y - height / 2) / (height / 2)) * 0.3f;
            glm::vec3 finalColor = color * yGrad;
            image.SetPixel(x, y, Pixel::FromVec3(finalColor));
        }
    }

    image.SavePNG(path);
}

// =============================================================================
// Mesh Primitive Generation
// =============================================================================

MeshData PlaceholderGenerator::GenerateBox(const glm::vec3& size, const glm::vec3& offset) {
    MeshData mesh;
    glm::vec3 half = size * 0.5f;

    // Define 8 corners
    std::array<glm::vec3, 8> corners = {{
        {-half.x, -half.y, -half.z},
        { half.x, -half.y, -half.z},
        { half.x,  half.y, -half.z},
        {-half.x,  half.y, -half.z},
        {-half.x, -half.y,  half.z},
        { half.x, -half.y,  half.z},
        { half.x,  half.y,  half.z},
        {-half.x,  half.y,  half.z}
    }};

    // Apply offset
    for (auto& c : corners) c += offset;

    // Face normals
    std::array<glm::vec3, 6> normals = {{
        { 0,  0, -1}, // Front
        { 0,  0,  1}, // Back
        {-1,  0,  0}, // Left
        { 1,  0,  0}, // Right
        { 0, -1,  0}, // Bottom
        { 0,  1,  0}  // Top
    }};

    // Face vertex indices (4 per face)
    std::array<std::array<int, 4>, 6> faceIndices = {{
        {0, 1, 2, 3}, // Front
        {5, 4, 7, 6}, // Back
        {4, 0, 3, 7}, // Left
        {1, 5, 6, 2}, // Right
        {4, 5, 1, 0}, // Bottom
        {3, 2, 6, 7}  // Top
    }};

    // UV coordinates for each face vertex
    std::array<glm::vec2, 4> uvs = {{
        {0, 0}, {1, 0}, {1, 1}, {0, 1}
    }};

    for (int f = 0; f < 6; ++f) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.vertices.size());

        for (int v = 0; v < 4; ++v) {
            Vertex vert;
            vert.position = corners[faceIndices[f][v]];
            vert.normal = normals[f];
            vert.texCoord = uvs[v];
            mesh.vertices.push_back(vert);
        }

        // Two triangles per face
        mesh.indices.push_back(baseIdx);
        mesh.indices.push_back(baseIdx + 1);
        mesh.indices.push_back(baseIdx + 2);
        mesh.indices.push_back(baseIdx);
        mesh.indices.push_back(baseIdx + 2);
        mesh.indices.push_back(baseIdx + 3);
    }

    return mesh;
}

MeshData PlaceholderGenerator::GenerateCylinder(float radius, float height, int segments, const glm::vec3& offset) {
    MeshData mesh;
    float halfH = height * 0.5f;

    // Generate vertices for top and bottom circles
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float u = static_cast<float>(i) / segments;

        // Bottom vertex
        Vertex vBottom;
        vBottom.position = glm::vec3(x, -halfH, z) + offset;
        vBottom.normal = glm::normalize(glm::vec3(x, 0, z));
        vBottom.texCoord = glm::vec2(u, 0);
        mesh.vertices.push_back(vBottom);

        // Top vertex
        Vertex vTop;
        vTop.position = glm::vec3(x, halfH, z) + offset;
        vTop.normal = glm::normalize(glm::vec3(x, 0, z));
        vTop.texCoord = glm::vec2(u, 1);
        mesh.vertices.push_back(vTop);
    }

    // Side faces
    for (int i = 0; i < segments; ++i) {
        uint32_t b0 = i * 2;
        uint32_t t0 = i * 2 + 1;
        uint32_t b1 = (i + 1) * 2;
        uint32_t t1 = (i + 1) * 2 + 1;

        mesh.indices.push_back(b0);
        mesh.indices.push_back(b1);
        mesh.indices.push_back(t0);
        mesh.indices.push_back(t0);
        mesh.indices.push_back(b1);
        mesh.indices.push_back(t1);
    }

    // Top cap
    uint32_t topCenter = static_cast<uint32_t>(mesh.vertices.size());
    Vertex vcTop;
    vcTop.position = glm::vec3(0, halfH, 0) + offset;
    vcTop.normal = glm::vec3(0, 1, 0);
    vcTop.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(vcTop);

    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        Vertex v;
        v.position = glm::vec3(x, halfH, z) + offset;
        v.normal = glm::vec3(0, 1, 0);
        v.texCoord = glm::vec2(x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(topCenter);
        mesh.indices.push_back(topCenter + 1 + i);
        mesh.indices.push_back(topCenter + 2 + i);
    }

    // Bottom cap
    uint32_t bottomCenter = static_cast<uint32_t>(mesh.vertices.size());
    Vertex vcBottom;
    vcBottom.position = glm::vec3(0, -halfH, 0) + offset;
    vcBottom.normal = glm::vec3(0, -1, 0);
    vcBottom.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(vcBottom);

    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        Vertex v;
        v.position = glm::vec3(x, -halfH, z) + offset;
        v.normal = glm::vec3(0, -1, 0);
        v.texCoord = glm::vec2(x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(bottomCenter);
        mesh.indices.push_back(bottomCenter + 2 + i);
        mesh.indices.push_back(bottomCenter + 1 + i);
    }

    return mesh;
}

MeshData PlaceholderGenerator::GenerateCone(float radius, float height, int segments, const glm::vec3& offset) {
    MeshData mesh;

    // Apex
    uint32_t apexIdx = 0;
    Vertex apex;
    apex.position = glm::vec3(0, height, 0) + offset;
    apex.normal = glm::vec3(0, 1, 0);
    apex.texCoord = glm::vec2(0.5f, 1.0f);
    mesh.vertices.push_back(apex);

    // Base circle vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        // Calculate normal for cone surface
        float slope = radius / height;
        glm::vec3 normal = glm::normalize(glm::vec3(x, slope * radius, z));

        Vertex v;
        v.position = glm::vec3(x, 0, z) + offset;
        v.normal = normal;
        v.texCoord = glm::vec2(static_cast<float>(i) / segments, 0);
        mesh.vertices.push_back(v);
    }

    // Side triangles
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(apexIdx);
        mesh.indices.push_back(1 + i);
        mesh.indices.push_back(2 + i);
    }

    // Bottom cap
    uint32_t centerIdx = static_cast<uint32_t>(mesh.vertices.size());
    Vertex center;
    center.position = glm::vec3(0, 0, 0) + offset;
    center.normal = glm::vec3(0, -1, 0);
    center.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(center);

    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        Vertex v;
        v.position = glm::vec3(x, 0, z) + offset;
        v.normal = glm::vec3(0, -1, 0);
        v.texCoord = glm::vec2(x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(centerIdx);
        mesh.indices.push_back(centerIdx + 2 + i);
        mesh.indices.push_back(centerIdx + 1 + i);
    }

    return mesh;
}

MeshData PlaceholderGenerator::GenerateSphere(float radius, int segments, int rings, const glm::vec3& offset) {
    MeshData mesh;

    for (int ring = 0; ring <= rings; ++ring) {
        float phi = glm::pi<float>() * static_cast<float>(ring) / rings;
        float y = std::cos(phi) * radius;
        float ringRadius = std::sin(phi) * radius;

        for (int seg = 0; seg <= segments; ++seg) {
            float theta = glm::two_pi<float>() * static_cast<float>(seg) / segments;
            float x = std::cos(theta) * ringRadius;
            float z = std::sin(theta) * ringRadius;

            Vertex v;
            v.position = glm::vec3(x, y, z) + offset;
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.texCoord = glm::vec2(
                static_cast<float>(seg) / segments,
                static_cast<float>(ring) / rings
            );
            mesh.vertices.push_back(v);
        }
    }

    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            mesh.indices.push_back(current);
            mesh.indices.push_back(next);
            mesh.indices.push_back(current + 1);

            mesh.indices.push_back(current + 1);
            mesh.indices.push_back(next);
            mesh.indices.push_back(next + 1);
        }
    }

    return mesh;
}

MeshData PlaceholderGenerator::GenerateHexPrism(float radius, float height, const glm::vec3& offset) {
    MeshData mesh;
    float halfH = height * 0.5f;

    // Generate hexagon vertices
    std::vector<glm::vec2> hexPoints;
    for (int i = 0; i < 6; ++i) {
        float angle = glm::pi<float>() / 6.0f + static_cast<float>(i) * glm::pi<float>() / 3.0f;
        hexPoints.push_back(glm::vec2(std::cos(angle) * radius, std::sin(angle) * radius));
    }

    // Top face
    uint32_t topCenter = static_cast<uint32_t>(mesh.vertices.size());
    Vertex vcTop;
    vcTop.position = glm::vec3(0, halfH, 0) + offset;
    vcTop.normal = glm::vec3(0, 1, 0);
    vcTop.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(vcTop);

    for (int i = 0; i < 6; ++i) {
        Vertex v;
        v.position = glm::vec3(hexPoints[i].x, halfH, hexPoints[i].y) + offset;
        v.normal = glm::vec3(0, 1, 0);
        v.texCoord = glm::vec2(hexPoints[i].x / radius * 0.5f + 0.5f, hexPoints[i].y / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < 6; ++i) {
        mesh.indices.push_back(topCenter);
        mesh.indices.push_back(topCenter + 1 + i);
        mesh.indices.push_back(topCenter + 1 + ((i + 1) % 6));
    }

    // Bottom face
    uint32_t bottomCenter = static_cast<uint32_t>(mesh.vertices.size());
    Vertex vcBottom;
    vcBottom.position = glm::vec3(0, -halfH, 0) + offset;
    vcBottom.normal = glm::vec3(0, -1, 0);
    vcBottom.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(vcBottom);

    for (int i = 0; i < 6; ++i) {
        Vertex v;
        v.position = glm::vec3(hexPoints[i].x, -halfH, hexPoints[i].y) + offset;
        v.normal = glm::vec3(0, -1, 0);
        v.texCoord = glm::vec2(hexPoints[i].x / radius * 0.5f + 0.5f, hexPoints[i].y / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < 6; ++i) {
        mesh.indices.push_back(bottomCenter);
        mesh.indices.push_back(bottomCenter + 1 + ((i + 1) % 6));
        mesh.indices.push_back(bottomCenter + 1 + i);
    }

    // Side faces
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        glm::vec3 p0(hexPoints[i].x, -halfH, hexPoints[i].y);
        glm::vec3 p1(hexPoints[next].x, -halfH, hexPoints[next].y);
        glm::vec3 p2(hexPoints[next].x, halfH, hexPoints[next].y);
        glm::vec3 p3(hexPoints[i].x, halfH, hexPoints[i].y);

        glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p3 - p0));

        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

        Vertex v0, v1, v2, v3;
        v0.position = p0 + offset; v0.normal = normal; v0.texCoord = glm::vec2(0, 0);
        v1.position = p1 + offset; v1.normal = normal; v1.texCoord = glm::vec2(1, 0);
        v2.position = p2 + offset; v2.normal = normal; v2.texCoord = glm::vec2(1, 1);
        v3.position = p3 + offset; v3.normal = normal; v3.texCoord = glm::vec2(0, 1);

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }

    return mesh;
}

MeshData PlaceholderGenerator::GeneratePyramid(float baseSize, float height, int sides, const glm::vec3& offset) {
    MeshData mesh;

    // Apex
    glm::vec3 apex(0, height, 0);
    float radius = baseSize * 0.5f;

    // Base vertices
    std::vector<glm::vec3> basePoints;
    for (int i = 0; i < sides; ++i) {
        float angle = static_cast<float>(i) / sides * glm::two_pi<float>();
        basePoints.push_back(glm::vec3(std::cos(angle) * radius, 0, std::sin(angle) * radius));
    }

    // Side faces
    for (int i = 0; i < sides; ++i) {
        int next = (i + 1) % sides;
        glm::vec3 p0 = basePoints[i];
        glm::vec3 p1 = basePoints[next];

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = apex - p0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

        Vertex v0, v1, v2;
        v0.position = p0 + offset; v0.normal = normal; v0.texCoord = glm::vec2(0, 0);
        v1.position = p1 + offset; v1.normal = normal; v1.texCoord = glm::vec2(1, 0);
        v2.position = apex + offset; v2.normal = normal; v2.texCoord = glm::vec2(0.5f, 1);

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);

        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
    }

    // Bottom face
    uint32_t centerIdx = static_cast<uint32_t>(mesh.vertices.size());
    Vertex center;
    center.position = glm::vec3(0, 0, 0) + offset;
    center.normal = glm::vec3(0, -1, 0);
    center.texCoord = glm::vec2(0.5f, 0.5f);
    mesh.vertices.push_back(center);

    for (int i = 0; i < sides; ++i) {
        Vertex v;
        v.position = basePoints[i] + offset;
        v.normal = glm::vec3(0, -1, 0);
        v.texCoord = glm::vec2(basePoints[i].x / radius * 0.5f + 0.5f, basePoints[i].z / radius * 0.5f + 0.5f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < sides; ++i) {
        int next = (i + 1) % sides;
        mesh.indices.push_back(centerIdx);
        mesh.indices.push_back(centerIdx + 1 + next);
        mesh.indices.push_back(centerIdx + 1 + i);
    }

    return mesh;
}

MeshData PlaceholderGenerator::CombineMeshes(const std::vector<MeshData>& meshes) {
    MeshData combined;

    for (const auto& mesh : meshes) {
        uint32_t indexOffset = static_cast<uint32_t>(combined.vertices.size());

        combined.vertices.insert(combined.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());

        for (uint32_t idx : mesh.indices) {
            combined.indices.push_back(idx + indexOffset);
        }
    }

    return combined;
}

void PlaceholderGenerator::TransformMesh(MeshData& mesh, const glm::mat4& transform) {
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    for (auto& v : mesh.vertices) {
        glm::vec4 pos = transform * glm::vec4(v.position, 1.0f);
        v.position = glm::vec3(pos);
        v.normal = glm::normalize(normalMatrix * v.normal);
    }
}

void PlaceholderGenerator::ScaleMesh(MeshData& mesh, const glm::vec3& scale) {
    TransformMesh(mesh, glm::scale(glm::mat4(1.0f), scale));
}

void PlaceholderGenerator::TranslateMesh(MeshData& mesh, const glm::vec3& offset) {
    for (auto& v : mesh.vertices) {
        v.position += offset;
    }
}

bool PlaceholderGenerator::WriteMeshToOBJ(const std::string& path, const MeshData& mesh, const std::string& materialName) {
    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }

    file << "# Placeholder model generated by Vehement2\n";
    file << "# Vertices: " << mesh.vertices.size() << "\n";
    file << "# Triangles: " << mesh.indices.size() / 3 << "\n\n";

    if (!materialName.empty()) {
        file << "mtllib " << materialName << ".mtl\n";
        file << "usemtl " << materialName << "\n\n";
    }

    // Write vertices
    for (const auto& v : mesh.vertices) {
        file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
    }
    file << "\n";

    // Write texture coordinates
    for (const auto& v : mesh.vertices) {
        file << "vt " << v.texCoord.x << " " << v.texCoord.y << "\n";
    }
    file << "\n";

    // Write normals
    for (const auto& v : mesh.vertices) {
        file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
    }
    file << "\n";

    // Write faces (1-indexed in OBJ format)
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i] + 1;
        uint32_t i1 = mesh.indices[i + 1] + 1;
        uint32_t i2 = mesh.indices[i + 2] + 1;
        file << "f " << i0 << "/" << i0 << "/" << i0 << " "
             << i1 << "/" << i1 << "/" << i1 << " "
             << i2 << "/" << i2 << "/" << i2 << "\n";
    }

    return true;
}

// =============================================================================
// Building Model Generation
// =============================================================================

void PlaceholderGenerator::GenerateBuildingModel(const std::string& path, BuildingType type) {
    MeshData mesh;

    switch (type) {
        case BuildingType::Shelter: {
            // Simple small cube house
            mesh = GenerateBox(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0, 1.0f, 0));
            // Add roof
            MeshData roof = GeneratePyramid(2.8f, 1.0f, 4, glm::vec3(0, 2.5f, 0));
            mesh = CombineMeshes({mesh, roof});
            break;
        }
        case BuildingType::House: {
            // Larger house with two floors
            MeshData base = GenerateBox(glm::vec3(4.0f, 3.0f, 3.0f), glm::vec3(0, 1.5f, 0));
            MeshData roof = GeneratePyramid(5.0f, 2.0f, 4, glm::vec3(0, 4.0f, 0));
            // Add chimney
            MeshData chimney = GenerateBox(glm::vec3(0.5f, 1.5f, 0.5f), glm::vec3(1.5f, 4.5f, 0.5f));
            mesh = CombineMeshes({base, roof, chimney});
            break;
        }
        case BuildingType::Barracks: {
            // Long building (2x1 hex)
            MeshData base = GenerateBox(glm::vec3(6.0f, 3.0f, 3.0f), glm::vec3(0, 1.5f, 0));
            // Simple sloped roof
            MeshData roof = GenerateBox(glm::vec3(6.5f, 0.5f, 4.0f), glm::vec3(0, 3.5f, 0));
            mesh = CombineMeshes({base, roof});
            break;
        }
        case BuildingType::Workshop: {
            // Industrial building with machinery
            MeshData main = GenerateBox(glm::vec3(4.0f, 4.0f, 4.0f), glm::vec3(0, 2.0f, 0));
            MeshData smokestack = GenerateCylinder(0.4f, 3.0f, 8, glm::vec3(1.5f, 5.5f, 1.5f));
            MeshData awning = GenerateBox(glm::vec3(2.0f, 0.3f, 1.5f), glm::vec3(0, 2.0f, 2.5f));
            mesh = CombineMeshes({main, smokestack, awning});
            break;
        }
        case BuildingType::Farm: {
            // Flat area with fences (2x2 hex)
            MeshData floor = GenerateBox(glm::vec3(8.0f, 0.2f, 8.0f), glm::vec3(0, 0.1f, 0));
            // Fences
            std::vector<MeshData> parts = {floor};
            for (int i = 0; i < 4; ++i) {
                float x = (i < 2) ? (i == 0 ? -4.0f : 4.0f) : 0;
                float z = (i >= 2) ? (i == 2 ? -4.0f : 4.0f) : 0;
                float length = (i < 2) ? 8.0f : 0.2f;
                float width = (i < 2) ? 0.2f : 8.0f;
                parts.push_back(GenerateBox(glm::vec3(length, 1.5f, width), glm::vec3(x, 0.75f, z)));
            }
            // Small barn
            parts.push_back(GenerateBox(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-2.0f, 1.0f, -2.0f)));
            mesh = CombineMeshes(parts);
            break;
        }
        case BuildingType::Watchtower: {
            // Tall narrow tower
            MeshData base = GenerateBox(glm::vec3(2.0f, 1.0f, 2.0f), glm::vec3(0, 0.5f, 0));
            MeshData tower = GenerateCylinder(0.8f, 6.0f, 8, glm::vec3(0, 4.0f, 0));
            MeshData platform = GenerateCylinder(1.2f, 0.3f, 8, glm::vec3(0, 7.15f, 0));
            MeshData roof = GenerateCone(1.5f, 1.5f, 8, glm::vec3(0, 8.0f, 0));
            mesh = CombineMeshes({base, tower, platform, roof});
            break;
        }
        case BuildingType::WallStraight: {
            // Hex edge wall segment
            mesh = GenerateBox(glm::vec3(4.0f, 3.0f, 0.5f), glm::vec3(0, 1.5f, 0));
            // Battlements
            for (int i = -1; i <= 1; ++i) {
                MeshData battlement = GenerateBox(glm::vec3(0.8f, 0.8f, 0.6f), glm::vec3(i * 1.4f, 3.4f, 0));
                mesh = CombineMeshes({mesh, battlement});
            }
            break;
        }
        case BuildingType::WallCorner: {
            // Hex corner wall
            MeshData wall1 = GenerateBox(glm::vec3(2.0f, 3.0f, 0.5f), glm::vec3(1.0f, 1.5f, 0));
            MeshData wall2 = GenerateBox(glm::vec3(0.5f, 3.0f, 2.0f), glm::vec3(0, 1.5f, 1.0f));
            MeshData corner = GenerateBox(glm::vec3(0.5f, 3.5f, 0.5f), glm::vec3(0, 1.75f, 0));
            mesh = CombineMeshes({wall1, wall2, corner});
            break;
        }
        case BuildingType::Gate: {
            // Wall with opening
            MeshData leftWall = GenerateBox(glm::vec3(1.2f, 3.0f, 0.5f), glm::vec3(-1.4f, 1.5f, 0));
            MeshData rightWall = GenerateBox(glm::vec3(1.2f, 3.0f, 0.5f), glm::vec3(1.4f, 1.5f, 0));
            MeshData archTop = GenerateBox(glm::vec3(1.6f, 1.0f, 0.5f), glm::vec3(0, 3.5f, 0));
            // Gate towers
            MeshData tower1 = GenerateBox(glm::vec3(1.0f, 4.5f, 1.0f), glm::vec3(-2.5f, 2.25f, 0));
            MeshData tower2 = GenerateBox(glm::vec3(1.0f, 4.5f, 1.0f), glm::vec3(2.5f, 2.25f, 0));
            mesh = CombineMeshes({leftWall, rightWall, archTop, tower1, tower2});
            break;
        }
        case BuildingType::Fortress: {
            // Large castle (3x3 hex)
            MeshData main = GenerateBox(glm::vec3(8.0f, 5.0f, 8.0f), glm::vec3(0, 2.5f, 0));
            std::vector<MeshData> parts = {main};
            // Corner towers
            for (int i = 0; i < 4; ++i) {
                float x = (i % 2 == 0) ? -4.5f : 4.5f;
                float z = (i < 2) ? -4.5f : 4.5f;
                parts.push_back(GenerateCylinder(1.2f, 7.0f, 8, glm::vec3(x, 3.5f, z)));
                parts.push_back(GenerateCone(1.5f, 2.0f, 8, glm::vec3(x, 8.0f, z)));
            }
            // Central keep
            parts.push_back(GenerateBox(glm::vec3(3.0f, 7.0f, 3.0f), glm::vec3(0, 3.5f, 0)));
            parts.push_back(GeneratePyramid(4.0f, 2.0f, 4, glm::vec3(0, 8.0f, 0)));
            mesh = CombineMeshes(parts);
            break;
        }
    }

    WriteMeshToOBJ(path, mesh, "building");
}

// =============================================================================
// Tree Model Generation
// =============================================================================

void PlaceholderGenerator::GenerateTreeModel(const std::string& path, TreeType type) {
    MeshData mesh;

    switch (type) {
        case TreeType::Pine: {
            // Conical pine tree
            MeshData trunk = GenerateCylinder(0.2f, 2.0f, 8, glm::vec3(0, 1.0f, 0));
            MeshData foliage1 = GenerateCone(1.5f, 2.0f, 8, glm::vec3(0, 2.5f, 0));
            MeshData foliage2 = GenerateCone(1.2f, 1.8f, 8, glm::vec3(0, 3.8f, 0));
            MeshData foliage3 = GenerateCone(0.9f, 1.5f, 8, glm::vec3(0, 5.0f, 0));
            mesh = CombineMeshes({trunk, foliage1, foliage2, foliage3});
            break;
        }
        case TreeType::Oak: {
            // Round oak tree
            MeshData trunk = GenerateCylinder(0.4f, 3.0f, 8, glm::vec3(0, 1.5f, 0));
            MeshData canopy = GenerateSphere(2.0f, 12, 8, glm::vec3(0, 4.5f, 0));
            // Add some branches
            MeshData branch1 = GenerateCylinder(0.15f, 1.0f, 6, glm::vec3(0.8f, 3.0f, 0));
            MeshData branch2 = GenerateCylinder(0.15f, 1.0f, 6, glm::vec3(-0.6f, 2.8f, 0.5f));
            mesh = CombineMeshes({trunk, canopy, branch1, branch2});
            break;
        }
    }

    WriteMeshToOBJ(path, mesh, "tree");
}

// =============================================================================
// Resource Model Generation
// =============================================================================

void PlaceholderGenerator::GenerateResourceModel(const std::string& path, ResourceType type) {
    MeshData mesh;

    switch (type) {
        case ResourceType::RockSmall: {
            // Small boulder - irregular sphere
            MeshData rock = GenerateSphere(0.5f, 8, 6, glm::vec3(0, 0.3f, 0));
            ScaleMesh(rock, glm::vec3(1.0f, 0.7f, 0.9f));
            mesh = rock;
            break;
        }
        case ResourceType::RockLarge: {
            // Large rock formation - multiple rocks
            MeshData rock1 = GenerateSphere(1.0f, 8, 6, glm::vec3(0, 0.6f, 0));
            ScaleMesh(rock1, glm::vec3(1.2f, 0.8f, 1.0f));
            MeshData rock2 = GenerateSphere(0.7f, 8, 6, glm::vec3(1.0f, 0.4f, 0.5f));
            MeshData rock3 = GenerateSphere(0.5f, 8, 6, glm::vec3(-0.8f, 0.3f, 0.3f));
            mesh = CombineMeshes({rock1, rock2, rock3});
            break;
        }
        case ResourceType::Bush: {
            // Shrub - sphere cluster
            MeshData center = GenerateSphere(0.6f, 8, 6, glm::vec3(0, 0.4f, 0));
            MeshData side1 = GenerateSphere(0.4f, 8, 6, glm::vec3(0.5f, 0.3f, 0));
            MeshData side2 = GenerateSphere(0.4f, 8, 6, glm::vec3(-0.4f, 0.3f, 0.3f));
            MeshData side3 = GenerateSphere(0.35f, 8, 6, glm::vec3(0, 0.3f, -0.4f));
            mesh = CombineMeshes({center, side1, side2, side3});
            break;
        }
        case ResourceType::Crate: {
            // Supply crate
            mesh = GenerateBox(glm::vec3(1.0f, 0.8f, 1.0f), glm::vec3(0, 0.4f, 0));
            // Add straps
            MeshData strap1 = GenerateBox(glm::vec3(1.1f, 0.1f, 0.1f), glm::vec3(0, 0.5f, 0.4f));
            MeshData strap2 = GenerateBox(glm::vec3(1.1f, 0.1f, 0.1f), glm::vec3(0, 0.5f, -0.4f));
            mesh = CombineMeshes({mesh, strap1, strap2});
            break;
        }
    }

    WriteMeshToOBJ(path, mesh, "resource");
}

// =============================================================================
// Unit Model Generation
// =============================================================================

void PlaceholderGenerator::GenerateUnitModel(const std::string& path, UnitType type) {
    MeshData mesh;

    // All units are simple humanoid shapes
    float scale = 1.0f;
    float headSize = 0.3f;
    float bodyHeight = 0.8f;
    float bodyWidth = 0.5f;
    float legHeight = 0.6f;
    float armLength = 0.6f;

    switch (type) {
        case UnitType::Hero: {
            scale = 1.2f;  // Heroes are larger
            break;
        }
        case UnitType::Worker: {
            scale = 0.9f;  // Workers are smaller
            break;
        }
        case UnitType::Zombie: {
            scale = 1.0f;
            break;
        }
        case UnitType::Guard: {
            scale = 1.1f;
            break;
        }
    }

    // Head
    MeshData head = GenerateSphere(headSize * scale, 8, 6, glm::vec3(0, (bodyHeight + legHeight + headSize) * scale, 0));

    // Body (torso)
    MeshData body = GenerateBox(glm::vec3(bodyWidth, bodyHeight, bodyWidth * 0.5f) * scale,
                                 glm::vec3(0, (legHeight + bodyHeight * 0.5f) * scale, 0));

    // Legs
    float legWidth = bodyWidth * 0.35f;
    MeshData leftLeg = GenerateBox(glm::vec3(legWidth, legHeight, legWidth) * scale,
                                    glm::vec3(-legWidth * 0.7f * scale, legHeight * 0.5f * scale, 0));
    MeshData rightLeg = GenerateBox(glm::vec3(legWidth, legHeight, legWidth) * scale,
                                     glm::vec3(legWidth * 0.7f * scale, legHeight * 0.5f * scale, 0));

    // Arms
    float armWidth = bodyWidth * 0.25f;
    MeshData leftArm = GenerateBox(glm::vec3(armWidth, armLength, armWidth) * scale,
                                    glm::vec3(-(bodyWidth * 0.5f + armWidth * 0.5f) * scale, (legHeight + bodyHeight * 0.6f) * scale, 0));
    MeshData rightArm = GenerateBox(glm::vec3(armWidth, armLength, armWidth) * scale,
                                     glm::vec3((bodyWidth * 0.5f + armWidth * 0.5f) * scale, (legHeight + bodyHeight * 0.6f) * scale, 0));

    std::vector<MeshData> parts = {head, body, leftLeg, rightLeg, leftArm, rightArm};

    // Add type-specific features
    switch (type) {
        case UnitType::Hero: {
            // Add cape
            MeshData cape = GenerateBox(glm::vec3(bodyWidth * 0.9f, bodyHeight * 1.2f, 0.1f) * scale,
                                        glm::vec3(0, (legHeight + bodyHeight * 0.5f) * scale, -bodyWidth * 0.3f * scale));
            parts.push_back(cape);
            break;
        }
        case UnitType::Worker: {
            // Add tool
            MeshData tool = GenerateBox(glm::vec3(0.1f, 0.8f, 0.1f) * scale,
                                        glm::vec3((bodyWidth * 0.5f + armWidth * 0.5f + 0.15f) * scale, (legHeight + bodyHeight * 0.3f) * scale, 0));
            parts.push_back(tool);
            break;
        }
        case UnitType::Zombie: {
            // Zombies have slightly modified posture (arms forward is implied by texture/animation)
            break;
        }
        case UnitType::Guard: {
            // Add shield
            MeshData shield = GenerateBox(glm::vec3(0.1f, 0.6f, 0.4f) * scale,
                                          glm::vec3(-(bodyWidth * 0.5f + armWidth + 0.1f) * scale, (legHeight + bodyHeight * 0.5f) * scale, 0));
            // Add spear
            MeshData spear = GenerateCylinder(0.05f * scale, 2.0f * scale, 6,
                                              glm::vec3((bodyWidth * 0.5f + armWidth * 0.5f + 0.1f) * scale, (legHeight + bodyHeight) * scale, 0));
            parts.push_back(shield);
            parts.push_back(spear);
            break;
        }
    }

    mesh = CombineMeshes(parts);
    WriteMeshToOBJ(path, mesh, "unit");
}

// =============================================================================
// Hex Tile Model Generation
// =============================================================================

void PlaceholderGenerator::GenerateHexTileModel(const std::string& path, TileType type) {
    // All hex tiles are flat hexagonal prisms
    float radius = 1.0f;  // Hex radius
    float height = 0.1f;  // Thin tile

    MeshData mesh = GenerateHexPrism(radius, height, glm::vec3(0, height * 0.5f, 0));

    // Add type-specific details
    switch (type) {
        case TileType::Grass: {
            // Add some small bumps for grass tufts
            for (int i = 0; i < 3; ++i) {
                float angle = static_cast<float>(i) * 2.1f;
                float r = 0.4f + static_cast<float>(i) * 0.15f;
                MeshData tuft = GenerateSphere(0.08f, 6, 4,
                    glm::vec3(std::cos(angle) * r, height + 0.05f, std::sin(angle) * r));
                mesh = CombineMeshes({mesh, tuft});
            }
            break;
        }
        case TileType::Dirt: {
            // Plain dirt, no extras
            break;
        }
        case TileType::Stone: {
            // Add some small rocks
            MeshData rock1 = GenerateSphere(0.1f, 6, 4, glm::vec3(0.3f, height + 0.05f, 0.2f));
            MeshData rock2 = GenerateSphere(0.08f, 6, 4, glm::vec3(-0.4f, height + 0.04f, -0.3f));
            mesh = CombineMeshes({mesh, rock1, rock2});
            break;
        }
        case TileType::Water: {
            // Make it slightly lower (represents water surface)
            // The water effect will be handled by shader/texture
            break;
        }
        case TileType::Road: {
            // Slightly raised edges
            MeshData edge = GenerateHexPrism(radius, height * 0.3f, glm::vec3(0, height + 0.05f, 0));
            ScaleMesh(edge, glm::vec3(1.05f, 1.0f, 1.05f));
            MeshData inner = GenerateHexPrism(radius * 0.9f, height * 0.5f, glm::vec3(0, height + 0.1f, 0));
            // We only keep the outer ring by using the main mesh
            break;
        }
    }

    WriteMeshToOBJ(path, mesh, "tile");
}

// =============================================================================
// Main Generation Functions
// =============================================================================

bool PlaceholderGenerator::AllPlaceholdersExist(const std::string& basePath) {
    std::filesystem::path base(basePath);

    // Check a sample of required files
    std::vector<std::string> requiredFiles = {
        "models/placeholders/shelter.obj",
        "models/placeholders/hero.obj",
        "models/placeholders/hex_grass.obj",
        "textures/placeholders/grass_diffuse.png",
        "textures/placeholders/brick_diffuse.png",
        "textures/placeholders/icon_food.png"
    };

    for (const auto& file : requiredFiles) {
        if (!std::filesystem::exists(base / file)) {
            return false;
        }
    }

    return true;
}

void PlaceholderGenerator::GenerateAllPlaceholders(const std::string& basePath, bool forceRegenerate) {
    std::filesystem::path base(basePath);

    if (!forceRegenerate && AllPlaceholdersExist(basePath)) {
        std::cout << "All placeholder assets already exist, skipping generation.\n";
        return;
    }

    std::cout << "Generating placeholder assets...\n";

    // Create directories
    std::filesystem::create_directories(base / "models/placeholders");
    std::filesystem::create_directories(base / "textures/placeholders");

    // ==========================================================================
    // Generate Building Models
    // ==========================================================================
    std::cout << "  Generating building models...\n";
    GenerateBuildingModel((base / "models/placeholders/shelter.obj").string(), BuildingType::Shelter);
    GenerateBuildingModel((base / "models/placeholders/house.obj").string(), BuildingType::House);
    GenerateBuildingModel((base / "models/placeholders/barracks.obj").string(), BuildingType::Barracks);
    GenerateBuildingModel((base / "models/placeholders/workshop.obj").string(), BuildingType::Workshop);
    GenerateBuildingModel((base / "models/placeholders/farm.obj").string(), BuildingType::Farm);
    GenerateBuildingModel((base / "models/placeholders/watchtower.obj").string(), BuildingType::Watchtower);
    GenerateBuildingModel((base / "models/placeholders/wall_straight.obj").string(), BuildingType::WallStraight);
    GenerateBuildingModel((base / "models/placeholders/wall_corner.obj").string(), BuildingType::WallCorner);
    GenerateBuildingModel((base / "models/placeholders/gate.obj").string(), BuildingType::Gate);
    GenerateBuildingModel((base / "models/placeholders/fortress.obj").string(), BuildingType::Fortress);

    // ==========================================================================
    // Generate Resource Models
    // ==========================================================================
    std::cout << "  Generating resource models...\n";
    GenerateTreeModel((base / "models/placeholders/tree_pine.obj").string(), TreeType::Pine);
    GenerateTreeModel((base / "models/placeholders/tree_oak.obj").string(), TreeType::Oak);
    GenerateResourceModel((base / "models/placeholders/rock_small.obj").string(), ResourceType::RockSmall);
    GenerateResourceModel((base / "models/placeholders/rock_large.obj").string(), ResourceType::RockLarge);
    GenerateResourceModel((base / "models/placeholders/bush.obj").string(), ResourceType::Bush);
    GenerateResourceModel((base / "models/placeholders/crate.obj").string(), ResourceType::Crate);

    // ==========================================================================
    // Generate Unit Models
    // ==========================================================================
    std::cout << "  Generating unit models...\n";
    GenerateUnitModel((base / "models/placeholders/hero.obj").string(), UnitType::Hero);
    GenerateUnitModel((base / "models/placeholders/worker.obj").string(), UnitType::Worker);
    GenerateUnitModel((base / "models/placeholders/zombie.obj").string(), UnitType::Zombie);
    GenerateUnitModel((base / "models/placeholders/guard.obj").string(), UnitType::Guard);

    // ==========================================================================
    // Generate Hex Tile Models
    // ==========================================================================
    std::cout << "  Generating hex tile models...\n";
    GenerateHexTileModel((base / "models/placeholders/hex_grass.obj").string(), TileType::Grass);
    GenerateHexTileModel((base / "models/placeholders/hex_dirt.obj").string(), TileType::Dirt);
    GenerateHexTileModel((base / "models/placeholders/hex_stone.obj").string(), TileType::Stone);
    GenerateHexTileModel((base / "models/placeholders/hex_water.obj").string(), TileType::Water);
    GenerateHexTileModel((base / "models/placeholders/hex_road.obj").string(), TileType::Road);

    // ==========================================================================
    // Generate Terrain Textures
    // ==========================================================================
    std::cout << "  Generating terrain textures...\n";
    GenerateNoiseTexture((base / "textures/placeholders/grass_diffuse.png").string(),
                         glm::vec3(0.2f, 0.5f, 0.15f), 256, 0.3f, 8.0f);
    GenerateNormalMap((base / "textures/placeholders/grass_normal.png").string(), 256, 0.1f);

    GenerateNoiseTexture((base / "textures/placeholders/dirt_diffuse.png").string(),
                         glm::vec3(0.45f, 0.35f, 0.2f), 256, 0.25f, 6.0f);

    GenerateNoiseTexture((base / "textures/placeholders/stone_diffuse.png").string(),
                         glm::vec3(0.5f, 0.5f, 0.5f), 256, 0.2f, 4.0f);

    GenerateWaterTexture((base / "textures/placeholders/water_diffuse.png").string(),
                         glm::vec3(0.1f, 0.3f, 0.6f), 256, 0.4f);

    GenerateRoadTexture((base / "textures/placeholders/road_diffuse.png").string(),
                        glm::vec3(0.35f, 0.35f, 0.35f), glm::vec3(0.9f, 0.9f, 0.8f), 256);

    // ==========================================================================
    // Generate Building Textures
    // ==========================================================================
    std::cout << "  Generating building textures...\n";
    GenerateWoodTexture((base / "textures/placeholders/wood_diffuse.png").string(),
                        glm::vec3(0.5f, 0.35f, 0.2f), 256, 25.0f);

    GenerateBrickTexture((base / "textures/placeholders/brick_diffuse.png").string(),
                         glm::vec3(0.6f, 0.25f, 0.2f), glm::vec3(0.7f, 0.7f, 0.65f), 256, 48, 24, 3);

    GenerateMetalTexture((base / "textures/placeholders/metal_diffuse.png").string(),
                         glm::vec3(0.6f, 0.6f, 0.65f), 256);

    GenerateThatchTexture((base / "textures/placeholders/thatch_diffuse.png").string(),
                          glm::vec3(0.7f, 0.6f, 0.3f), 256);

    // ==========================================================================
    // Generate UI Textures
    // ==========================================================================
    std::cout << "  Generating UI textures...\n";
    GenerateIcon((base / "textures/placeholders/icon_food.png").string(),
                 "food", glm::vec3(0.8f, 0.2f, 0.2f), 64);
    GenerateIcon((base / "textures/placeholders/icon_wood.png").string(),
                 "wood", glm::vec3(0.5f, 0.35f, 0.2f), 64);
    GenerateIcon((base / "textures/placeholders/icon_stone.png").string(),
                 "stone", glm::vec3(0.5f, 0.5f, 0.5f), 64);
    GenerateIcon((base / "textures/placeholders/icon_metal.png").string(),
                 "metal", glm::vec3(0.7f, 0.7f, 0.75f), 64);
    GenerateIcon((base / "textures/placeholders/icon_coins.png").string(),
                 "coins", glm::vec3(0.9f, 0.75f, 0.2f), 64);

    GenerateBarTexture((base / "textures/placeholders/health_bar.png").string(),
                       glm::vec3(0.8f, 0.2f, 0.2f), glm::vec3(0.2f, 0.2f, 0.2f), 256, 32);
    GenerateBarTexture((base / "textures/placeholders/mana_bar.png").string(),
                       glm::vec3(0.2f, 0.4f, 0.9f), glm::vec3(0.2f, 0.2f, 0.2f), 256, 32);

    std::cout << "Placeholder asset generation complete!\n";
}

} // namespace Vehement
