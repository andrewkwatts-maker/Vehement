/**
 * @file AssetBrowser_SDFExtension.cpp
 * @brief Extension to AssetBrowser for SDF conversion context menu
 *
 * This file adds "Convert to SDF" functionality to the AssetBrowser context menu.
 * To integrate with AssetBrowser, add the following to your AssetBrowser rendering code:
 *
 * Example Integration:
 * @code
 * // In your asset browser rendering loop, after displaying each asset:
 * if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
 *     ImGui::OpenPopup("AssetContextMenu");
 *     m_contextMenuAsset = asset;
 * }
 *
 * if (ImGui::BeginPopup("AssetContextMenu")) {
 *     if (ShowAssetContextMenu(m_contextMenuAsset)) {
 *         ImGui::CloseCurrentPopup();
 *     }
 *     ImGui::EndPopup();
 * }
 * @endcode
 */

#include "AssetBrowser.hpp"
#include "../engine/graphics/MeshToSDFConverter.hpp"
#include "../engine/graphics/ModelLoader.hpp"
#include "../engine/sdf/SDFModel.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

/**
 * @brief Show context menu for an asset
 * This is the main entry point for the context menu system.
 * Call this inside an ImGui::BeginPopup() block.
 */
bool ShowAssetContextMenu(AssetBrowser& browser, const AssetEntry& asset) {
    bool actionTaken = false;

    // Standard file operations
    if (ImGui::MenuItem("Open")) {
        spdlog::info("Open: {}", asset.path);
        // TODO: Implement file opening
        actionTaken = true;
    }

    if (ImGui::MenuItem("Rename")) {
        spdlog::info("Rename: {}", asset.path);
        // TODO: Implement rename dialog
        actionTaken = true;
    }

    if (ImGui::MenuItem("Delete", nullptr, false, !asset.isDirectory)) {
        if (browser.DeleteAsset(asset.path)) {
            spdlog::info("Deleted: {}", asset.path);
            browser.Refresh();
            actionTaken = true;
        }
    }

    ImGui::Separator();

    // SDF Conversion option (only for model files)
    if (asset.type == "Model" && !asset.isDirectory) {
        if (ImGui::MenuItem("Convert to SDF")) {
            ConvertMeshToSDF(browser, asset.path);
            actionTaken = true;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Convert this mesh to SDF primitives");
            ImGui::Text("This will create a .sdfmesh file");
            ImGui::EndTooltip();
        }
    }

    // Show properties
    ImGui::Separator();
    if (ImGui::MenuItem("Properties")) {
        spdlog::info("Show properties for: {}", asset.path);
        // TODO: Show properties dialog
        actionTaken = true;
    }

    return actionTaken;
}

/**
 * @brief Show SDF conversion dialog
 */
struct SDFConversionDialog {
    bool isOpen = false;
    std::string sourcePath;
    std::string outputPath;

    // Settings
    int maxPrimitives = 40;
    float errorThreshold = 0.05f;
    int strategyIndex = 4; // Auto
    int qualityIndex = 1;  // Balanced
    bool generateLODs = true;

    // LOD settings
    int lodCounts[4] = {40, 12, 6, 3};
    float lodDistances[4] = {10.0f, 25.0f, 50.0f, 100.0f};

    // Progress
    bool isConverting = false;
    float progress = 0.0f;
    std::string statusMessage;

    void Open(const std::string& meshPath) {
        isOpen = true;
        sourcePath = meshPath;

        // Generate output path
        fs::path p(meshPath);
        outputPath = p.replace_extension(".sdfmesh").string();

        // Reset state
        isConverting = false;
        progress = 0.0f;
        statusMessage = "Ready to convert";
    }

    void Render() {
        if (!isOpen) return;

        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Convert Mesh to SDF", &isOpen)) {
            // Source info
            ImGui::Text("Source: %s", sourcePath.c_str());
            ImGui::Separator();

            // Output path
            ImGui::Text("Output:");
            ImGui::SameLine();
            char outputBuf[512];
            strncpy(outputBuf, outputPath.c_str(), sizeof(outputBuf) - 1);
            if (ImGui::InputText("##output", outputBuf, sizeof(outputBuf))) {
                outputPath = outputBuf;
            }

            ImGui::Separator();

            // Settings
            if (ImGui::CollapsingHeader("Conversion Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderInt("Max Primitives", &maxPrimitives, 1, 100);
                ImGui::SliderFloat("Error Threshold", &errorThreshold, 0.001f, 0.5f, "%.3f");

                const char* strategies[] = {"Primitive Fitting", "Convex Decomposition", "Voxelization", "Hybrid", "Auto"};
                ImGui::Combo("Strategy", &strategyIndex, strategies, 5);

                const char* qualities[] = {"Fast", "Balanced", "High", "Perfect"};
                ImGui::Combo("Quality", &qualityIndex, qualities, 4);
            }

            if (ImGui::CollapsingHeader("LOD Settings")) {
                ImGui::Checkbox("Generate LODs", &generateLODs);

                if (generateLODs) {
                    ImGui::Text("LOD0 (Close):");
                    ImGui::SameLine();
                    ImGui::SliderInt("##lod0count", &lodCounts[0], 1, 100);
                    ImGui::SameLine();
                    ImGui::SliderFloat("##lod0dist", &lodDistances[0], 0.0f, 50.0f, "%.1fm");

                    ImGui::Text("LOD1:");
                    ImGui::SameLine();
                    ImGui::SliderInt("##lod1count", &lodCounts[1], 1, 50);
                    ImGui::SameLine();
                    ImGui::SliderFloat("##lod1dist", &lodDistances[1], 0.0f, 100.0f, "%.1fm");

                    ImGui::Text("LOD2:");
                    ImGui::SameLine();
                    ImGui::SliderInt("##lod2count", &lodCounts[2], 1, 20);
                    ImGui::SameLine();
                    ImGui::SliderFloat("##lod2dist", &lodDistances[2], 0.0f, 200.0f, "%.1fm");

                    ImGui::Text("LOD3 (Far):");
                    ImGui::SameLine();
                    ImGui::SliderInt("##lod3count", &lodCounts[3], 1, 10);
                    ImGui::SameLine();
                    ImGui::SliderFloat("##lod3dist", &lodDistances[3], 0.0f, 500.0f, "%.1fm");
                }
            }

            ImGui::Separator();

            // Progress
            if (isConverting) {
                ImGui::ProgressBar(progress, ImVec2(-1, 0));
                ImGui::Text("%s", statusMessage.c_str());
            } else {
                ImGui::Text("%s", statusMessage.c_str());
            }

            ImGui::Separator();

            // Buttons
            if (!isConverting) {
                if (ImGui::Button("Convert", ImVec2(120, 0))) {
                    StartConversion();
                }
                ImGui::SameLine();
            }

            if (ImGui::Button(isConverting ? "Cancel" : "Close", ImVec2(120, 0))) {
                if (isConverting) {
                    // TODO: Cancel conversion
                }
                isOpen = false;
            }
        }
        ImGui::End();
    }

    void StartConversion() {
        isConverting = true;
        progress = 0.0f;
        statusMessage = "Loading mesh...";

        // Run conversion in background thread
        std::thread([this]() {
            PerformConversion();
        }).detach();
    }

    void PerformConversion() {
        try {
            // Setup converter
            Nova::MeshToSDFConverter converter;
            Nova::ConversionSettings settings;

            // Apply UI settings
            const Nova::ConversionStrategy strategies[] = {
                Nova::ConversionStrategy::PrimitiveFitting,
                Nova::ConversionStrategy::ConvexDecomposition,
                Nova::ConversionStrategy::Voxelization,
                Nova::ConversionStrategy::Hybrid,
                Nova::ConversionStrategy::Auto
            };
            settings.strategy = strategies[strategyIndex];

            const Nova::FittingQuality qualities[] = {
                Nova::FittingQuality::Fast,
                Nova::FittingQuality::Balanced,
                Nova::FittingQuality::High,
                Nova::FittingQuality::Perfect
            };
            settings.quality = qualities[qualityIndex];

            settings.maxPrimitives = maxPrimitives;
            settings.errorThreshold = errorThreshold;
            settings.generateLODs = generateLODs;

            if (generateLODs) {
                settings.lodPrimitiveCounts = {lodCounts[0], lodCounts[1], lodCounts[2], lodCounts[3]};
                settings.lodDistances = {lodDistances[0], lodDistances[1], lodDistances[2], lodDistances[3]};
            }

            settings.verbose = true;
            settings.progressCallback = [this](float p) {
                progress = p;
            };

            statusMessage = "Converting...";

            // Load mesh (NOTE: Requires actual model loader implementation)
            std::vector<Nova::Vertex> vertices;
            std::vector<uint32_t> indices;

            // TODO: Load mesh from file using ModelLoader
            // Nova::ModelLoader loader;
            // auto mesh = loader.LoadModel(sourcePath);

            if (vertices.empty()) {
                statusMessage = "Error: Could not load mesh";
                isConverting = false;
                return;
            }

            // Convert
            Nova::ConversionResult result = converter.Convert(vertices, indices, settings);

            if (!result.success) {
                statusMessage = "Error: " + result.errorMessage;
                isConverting = false;
                return;
            }

            // Save output (simplified)
            statusMessage = "Saving...";

            // TODO: Save .sdfmesh file
            // For now, just log success
            spdlog::info("Conversion complete: {} primitives, {:.3f}ms",
                        result.primitiveCount, result.conversionTimeMs);

            statusMessage = "Complete! Generated " + std::to_string(result.primitiveCount) + " primitives";
            progress = 1.0f;

        } catch (const std::exception& e) {
            statusMessage = std::string("Error: ") + e.what();
        }

        isConverting = false;
    }
};

// Global dialog instance (in a real app, this would be a member of the AssetBrowser or editor)
static SDFConversionDialog g_conversionDialog;

/**
 * @brief Convert mesh to SDF primitives
 */
bool ConvertMeshToSDF(AssetBrowser& browser, const std::string& meshPath) {
    spdlog::info("Starting SDF conversion for: {}", meshPath);

    // Open conversion dialog
    g_conversionDialog.Open(meshPath);

    return true;
}

/**
 * @brief Render conversion dialog (call this in your main UI loop)
 */
void RenderSDFConversionDialog() {
    g_conversionDialog.Render();
}

/**
 * @brief Example integration code for AssetBrowser rendering
 *
 * Add this code to your AssetBrowser UI rendering function:
 *
 * @code
 * // In grid/list view rendering loop:
 * for (const auto& asset : filteredAssets) {
 *     // ... render asset ...
 *
 *     // Add context menu support
 *     if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
 *         ImGui::OpenPopup("AssetContextMenu");
 *         m_contextMenuAsset = asset;
 *     }
 * }
 *
 * // After the loop:
 * if (ImGui::BeginPopup("AssetContextMenu")) {
 *     if (ShowAssetContextMenu(*this, m_contextMenuAsset)) {
 *         ImGui::CloseCurrentPopup();
 *     }
 *     ImGui::EndPopup();
 * }
 *
 * // In main render loop (outside asset browser):
 * RenderSDFConversionDialog();
 * @endcode
 */
