/**
 * @file MeshToSDFTool.cpp
 * @brief Standalone command-line tool for converting meshes to SDF primitives
 *
 * Usage:
 *   MeshToSDFTool input.obj output.sdfmesh [options]
 *
 * Options:
 *   --strategy <strategy>       Conversion strategy (primitive, convex, voxel, auto)
 *   --quality <quality>         Fitting quality (fast, balanced, high, perfect)
 *   --max-primitives <count>    Maximum number of primitives (default: 40)
 *   --error-threshold <value>   Error threshold 0-1 (default: 0.05)
 *   --generate-lods             Generate LOD levels
 *   --lod-counts <list>         LOD primitive counts (e.g., "40,12,6,3")
 *   --lod-distances <list>      LOD distances (e.g., "10,25,50,100")
 *   --bind-skeleton <file>      Bind to skeleton file
 *   --preview                   Show preview window
 *   --verbose                   Print detailed progress
 *   --help                      Show this help
 */

#include "../engine/graphics/MeshToSDFConverter.hpp"
#include "../engine/graphics/ModelLoader.hpp"
#include "../engine/sdf/SDFModel.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// Configuration
// ============================================================================

struct ToolConfig {
    std::string inputFile;
    std::string outputFile;
    std::string skeletonFile;

    Nova::ConversionStrategy strategy = Nova::ConversionStrategy::Auto;
    Nova::FittingQuality quality = Nova::FittingQuality::Balanced;

    int maxPrimitives = 40;
    float errorThreshold = 0.05f;

    bool generateLODs = false;
    std::vector<int> lodCounts = {40, 12, 6, 3};
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};

    bool bindSkeleton = false;
    bool preview = false;
    bool verbose = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

void PrintUsage() {
    std::cout << R"(
MeshToSDFTool - Convert triangle meshes to SDF primitives

Usage:
  MeshToSDFTool input.obj output.sdfmesh [options]

Options:
  --strategy <strategy>       Conversion strategy
                              (primitive, convex, voxel, hybrid, auto)
                              Default: auto

  --quality <quality>         Fitting quality
                              (fast, balanced, high, perfect)
                              Default: balanced

  --max-primitives <count>    Maximum number of primitives
                              Default: 40

  --error-threshold <value>   Error threshold 0-1
                              Default: 0.05

  --generate-lods             Generate LOD levels

  --lod-counts <list>         LOD primitive counts
                              Example: "40,12,6,3"

  --lod-distances <list>      LOD distances in meters
                              Example: "10,25,50,100"

  --bind-skeleton <file>      Bind to skeleton file (.skeleton)

  --preview                   Show preview window (requires OpenGL)

  --verbose                   Print detailed progress

  --help                      Show this help

Examples:
  # Basic conversion
  MeshToSDFTool character.obj character.sdfmesh

  # High quality with LODs
  MeshToSDFTool character.obj character.sdfmesh --quality high --generate-lods

  # Custom primitive count
  MeshToSDFTool prop.obj prop.sdfmesh --max-primitives 20

  # Bind to skeleton
  MeshToSDFTool character.obj character.sdfmesh --bind-skeleton character.skeleton

Output Format:
  .sdfmesh files contain:
  - JSON metadata (LOD levels, bone bindings, primitive types)
  - Binary blob with primitive parameters
  - Compatible with Nova3D engine runtime
)";
}

std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<int> ParseIntList(const std::string& str) {
    std::vector<int> result;
    auto tokens = SplitString(str, ',');

    for (const auto& token : tokens) {
        try {
            result.push_back(std::stoi(token));
        } catch (...) {
            std::cerr << "Warning: Could not parse integer: " << token << "\n";
        }
    }

    return result;
}

std::vector<float> ParseFloatList(const std::string& str) {
    std::vector<float> result;
    auto tokens = SplitString(str, ',');

    for (const auto& token : tokens) {
        try {
            result.push_back(std::stof(token));
        } catch (...) {
            std::cerr << "Warning: Could not parse float: " << token << "\n";
        }
    }

    return result;
}

Nova::ConversionStrategy ParseStrategy(const std::string& str) {
    if (str == "primitive") return Nova::ConversionStrategy::PrimitiveFitting;
    if (str == "convex") return Nova::ConversionStrategy::ConvexDecomposition;
    if (str == "voxel") return Nova::ConversionStrategy::Voxelization;
    if (str == "hybrid") return Nova::ConversionStrategy::Hybrid;
    return Nova::ConversionStrategy::Auto;
}

Nova::FittingQuality ParseQuality(const std::string& str) {
    if (str == "fast") return Nova::FittingQuality::Fast;
    if (str == "balanced") return Nova::FittingQuality::Balanced;
    if (str == "high") return Nova::FittingQuality::High;
    if (str == "perfect") return Nova::FittingQuality::Perfect;
    return Nova::FittingQuality::Balanced;
}

// ============================================================================
// Argument Parsing
// ============================================================================

bool ParseArguments(int argc, char* argv[], ToolConfig& config) {
    if (argc < 3) {
        return false;
    }

    config.inputFile = argv[1];
    config.outputFile = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        else if (arg == "--strategy" && i + 1 < argc) {
            config.strategy = ParseStrategy(argv[++i]);
        }
        else if (arg == "--quality" && i + 1 < argc) {
            config.quality = ParseQuality(argv[++i]);
        }
        else if (arg == "--max-primitives" && i + 1 < argc) {
            config.maxPrimitives = std::atoi(argv[++i]);
        }
        else if (arg == "--error-threshold" && i + 1 < argc) {
            config.errorThreshold = std::atof(argv[++i]);
        }
        else if (arg == "--generate-lods") {
            config.generateLODs = true;
        }
        else if (arg == "--lod-counts" && i + 1 < argc) {
            config.lodCounts = ParseIntList(argv[++i]);
        }
        else if (arg == "--lod-distances" && i + 1 < argc) {
            config.lodDistances = ParseFloatList(argv[++i]);
        }
        else if (arg == "--bind-skeleton" && i + 1 < argc) {
            config.skeletonFile = argv[++i];
            config.bindSkeleton = true;
        }
        else if (arg == "--preview") {
            config.preview = true;
        }
        else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
        }
    }

    return true;
}

// ============================================================================
// File I/O
// ============================================================================

bool SaveSDFMesh(const std::string& filename, const Nova::ConversionResult& result) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open output file: " << filename << "\n";
        return false;
    }

    // Write magic number
    const uint32_t magic = 0x5344464D; // "SDFM"
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

    // Write version
    const uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write primitive count
    uint32_t primitiveCount = static_cast<uint32_t>(result.allPrimitives.size());
    file.write(reinterpret_cast<const char*>(&primitiveCount), sizeof(primitiveCount));

    // Write primitives
    for (const auto& prim : result.allPrimitives) {
        // Write primitive type
        uint32_t type = static_cast<uint32_t>(prim.type);
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));

        // Write transform
        file.write(reinterpret_cast<const char*>(&prim.position), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&prim.orientation), sizeof(glm::quat));
        file.write(reinterpret_cast<const char*>(&prim.scale), sizeof(glm::vec3));

        // Write parameters
        file.write(reinterpret_cast<const char*>(&prim.parameters), sizeof(Nova::SDFParameters));

        // Write metrics
        file.write(reinterpret_cast<const char*>(&prim.error), sizeof(float));
        file.write(reinterpret_cast<const char*>(&prim.coverage), sizeof(float));
        file.write(reinterpret_cast<const char*>(&prim.importance), sizeof(float));
    }

    // Write LOD levels
    uint32_t lodLevelCount = static_cast<uint32_t>(result.lodLevels.size());
    file.write(reinterpret_cast<const char*>(&lodLevelCount), sizeof(lodLevelCount));

    for (const auto& lodLevel : result.lodLevels) {
        uint32_t indexCount = static_cast<uint32_t>(lodLevel.size());
        file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));

        for (int idx : lodLevel) {
            uint32_t index = static_cast<uint32_t>(idx);
            file.write(reinterpret_cast<const char*>(&index), sizeof(index));
        }
    }

    file.close();
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "MeshToSDFTool v1.0\n";
    std::cout << "==================\n\n";

    // Parse arguments
    ToolConfig config;
    if (!ParseArguments(argc, argv, config)) {
        PrintUsage();
        return 1;
    }

    // Check input file exists
    if (!fs::exists(config.inputFile)) {
        std::cerr << "Error: Input file does not exist: " << config.inputFile << "\n";
        return 1;
    }

    // Load mesh
    std::cout << "Loading mesh: " << config.inputFile << "\n";

    // Note: In a real implementation, we would use the ModelLoader
    // For this example, we'll create dummy data
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    // TODO: Load actual mesh data using ModelLoader
    // Nova::ModelLoader loader;
    // auto mesh = loader.LoadModel(config.inputFile);

    if (vertices.empty()) {
        std::cerr << "Error: Could not load mesh or mesh is empty\n";
        return 1;
    }

    std::cout << "  Vertices: " << vertices.size() << "\n";
    std::cout << "  Triangles: " << indices.size() / 3 << "\n\n";

    // Setup conversion settings
    Nova::ConversionSettings settings;
    settings.strategy = config.strategy;
    settings.quality = config.quality;
    settings.maxPrimitives = config.maxPrimitives;
    settings.errorThreshold = config.errorThreshold;
    settings.generateLODs = config.generateLODs;
    settings.lodPrimitiveCounts = config.lodCounts;
    settings.lodDistances = config.lodDistances;
    settings.verbose = config.verbose;

    // Progress callback
    if (config.verbose) {
        settings.progressCallback = [](float progress) {
            static int lastPercent = -1;
            int percent = static_cast<int>(progress * 100);
            if (percent != lastPercent) {
                std::cout << "\rProgress: " << percent << "%" << std::flush;
                lastPercent = percent;
            }
        };
    }

    // Convert
    std::cout << "Converting mesh to SDF primitives...\n";
    std::cout << "  Strategy: " << static_cast<int>(config.strategy) << "\n";
    std::cout << "  Quality: " << static_cast<int>(config.quality) << "\n";
    std::cout << "  Max Primitives: " << config.maxPrimitives << "\n\n";

    Nova::MeshToSDFConverter converter;
    Nova::ConversionResult result = converter.Convert(vertices, indices, settings);

    if (config.verbose) {
        std::cout << "\n";
    }

    if (!result.success) {
        std::cerr << "Error: Conversion failed: " << result.errorMessage << "\n";
        return 1;
    }

    // Print results
    std::cout << "Conversion complete!\n";
    std::cout << "  Primitives generated: " << result.primitiveCount << "\n";
    std::cout << "  Average error: " << result.avgError << "\n";
    std::cout << "  Max error: " << result.maxError << "\n";
    std::cout << "  Conversion time: " << result.conversionTimeMs << " ms\n";

    if (config.generateLODs) {
        std::cout << "  LOD levels: " << result.lodLevels.size() << "\n";
        for (size_t i = 0; i < result.lodLevels.size(); ++i) {
            std::cout << "    LOD" << i << ": " << result.lodLevels[i].size() << " primitives\n";
        }
    }

    std::cout << "\n";

    // Save output
    std::cout << "Saving to: " << config.outputFile << "\n";
    if (!SaveSDFMesh(config.outputFile, result)) {
        return 1;
    }

    std::cout << "Done!\n";

    return 0;
}
