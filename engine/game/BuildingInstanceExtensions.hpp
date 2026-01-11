#pragma once

#include "BuildingComponentSystem.hpp"
#include "BuildingUI.hpp"
#include <memory>
#include <vector>
#include <string>

namespace Nova {
namespace Buildings {

/**
 * @brief Extension methods for BuildingInstance to add production and UI support
 */
class BuildingInstanceExtensions {
public:
    // Add production queue support to a building instance
    static void InitializeProductionQueue(BuildingInstance& building);

    // Add UI state support to a building instance
    static void InitializeUIState(BuildingInstance& building);

    // Get or create production queue
    static std::shared_ptr<ProductionQueue> GetOrCreateProductionQueue(BuildingInstance& building);

    // Get or create UI state
    static std::shared_ptr<BuildingUIState> GetOrCreateUIState(BuildingInstance& building);

    // Research management
    static void AddCompletedResearch(BuildingInstance& building, const std::string& techId);
    static bool HasResearch(const BuildingInstance& building, const std::string& techId);
    static const std::vector<std::string>& GetCompletedResearch(const BuildingInstance& building);

    // Update building systems
    static void Update(BuildingInstance& building, float deltaTime);
};

// Helper class to store extended building data
class BuildingExtendedData {
public:
    std::shared_ptr<ProductionQueue> productionQueue;
    std::vector<std::string> completedResearch;
    std::shared_ptr<BuildingUIState> uiState;

    nlohmann::json Serialize() const;
    static std::shared_ptr<BuildingExtendedData> Deserialize(const nlohmann::json& json);
};

// Global registry for extended building data
class BuildingDataRegistry {
public:
    static BuildingDataRegistry& Instance();

    void RegisterBuildingData(const std::string& buildingId, std::shared_ptr<BuildingExtendedData> data);
    std::shared_ptr<BuildingExtendedData> GetBuildingData(const std::string& buildingId);
    std::shared_ptr<BuildingExtendedData> GetOrCreateBuildingData(const std::string& buildingId);

    void RemoveBuildingData(const std::string& buildingId);
    void Clear();

private:
    BuildingDataRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<BuildingExtendedData>> m_buildingData;
};

} // namespace Buildings
} // namespace Nova
