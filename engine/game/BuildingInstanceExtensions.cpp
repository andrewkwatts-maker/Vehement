#include "BuildingInstanceExtensions.hpp"
#include <algorithm>

namespace Nova {
namespace Buildings {

// =============================================================================
// BuildingInstanceExtensions Implementation
// =============================================================================

void BuildingInstanceExtensions::InitializeProductionQueue(BuildingInstance& building) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    if (!data->productionQueue) {
        data->productionQueue = std::make_shared<ProductionQueue>();
    }
}

void BuildingInstanceExtensions::InitializeUIState(BuildingInstance& building) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    if (!data->uiState) {
        data->uiState = std::make_shared<BuildingUIState>();
    }
}

std::shared_ptr<ProductionQueue> BuildingInstanceExtensions::GetOrCreateProductionQueue(BuildingInstance& building) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    if (!data->productionQueue) {
        data->productionQueue = std::make_shared<ProductionQueue>();
    }
    return data->productionQueue;
}

std::shared_ptr<BuildingUIState> BuildingInstanceExtensions::GetOrCreateUIState(BuildingInstance& building) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    if (!data->uiState) {
        data->uiState = std::make_shared<BuildingUIState>();
    }
    return data->uiState;
}

void BuildingInstanceExtensions::AddCompletedResearch(BuildingInstance& building, const std::string& techId) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    if (std::find(data->completedResearch.begin(), data->completedResearch.end(), techId) == data->completedResearch.end()) {
        data->completedResearch.push_back(techId);
    }
}

bool BuildingInstanceExtensions::HasResearch(const BuildingInstance& building, const std::string& techId) {
    auto data = BuildingDataRegistry::Instance().GetBuildingData(building.GetId());
    if (!data) return false;

    return std::find(data->completedResearch.begin(), data->completedResearch.end(), techId) != data->completedResearch.end();
}

const std::vector<std::string>& BuildingInstanceExtensions::GetCompletedResearch(const BuildingInstance& building) {
    auto data = BuildingDataRegistry::Instance().GetOrCreateBuildingData(building.GetId());
    return data->completedResearch;
}

void BuildingInstanceExtensions::Update(BuildingInstance& building, float deltaTime) {
    auto data = BuildingDataRegistry::Instance().GetBuildingData(building.GetId());
    if (!data) return;

    // Update production queue
    if (data->productionQueue) {
        data->productionQueue->Update(deltaTime);
    }
}

// =============================================================================
// BuildingExtendedData Implementation
// =============================================================================

nlohmann::json BuildingExtendedData::Serialize() const {
    nlohmann::json json;

    if (productionQueue) {
        json["productionQueue"] = productionQueue->Serialize();
    }

    json["completedResearch"] = completedResearch;

    if (uiState) {
        json["uiState"] = uiState->Serialize();
    }

    return json;
}

std::shared_ptr<BuildingExtendedData> BuildingExtendedData::Deserialize(const nlohmann::json& json) {
    auto data = std::make_shared<BuildingExtendedData>();

    if (json.contains("productionQueue")) {
        data->productionQueue = ProductionQueue::Deserialize(json["productionQueue"]);
    }

    data->completedResearch = json.value("completedResearch", std::vector<std::string>{});

    if (json.contains("uiState")) {
        data->uiState = BuildingUIState::Deserialize(json["uiState"]);
    }

    return data;
}

// =============================================================================
// BuildingDataRegistry Implementation
// =============================================================================

BuildingDataRegistry& BuildingDataRegistry::Instance() {
    static BuildingDataRegistry instance;
    return instance;
}

void BuildingDataRegistry::RegisterBuildingData(const std::string& buildingId, std::shared_ptr<BuildingExtendedData> data) {
    m_buildingData[buildingId] = data;
}

std::shared_ptr<BuildingExtendedData> BuildingDataRegistry::GetBuildingData(const std::string& buildingId) {
    auto it = m_buildingData.find(buildingId);
    return it != m_buildingData.end() ? it->second : nullptr;
}

std::shared_ptr<BuildingExtendedData> BuildingDataRegistry::GetOrCreateBuildingData(const std::string& buildingId) {
    auto data = GetBuildingData(buildingId);
    if (!data) {
        data = std::make_shared<BuildingExtendedData>();
        RegisterBuildingData(buildingId, data);
    }
    return data;
}

void BuildingDataRegistry::RemoveBuildingData(const std::string& buildingId) {
    m_buildingData.erase(buildingId);
}

void BuildingDataRegistry::Clear() {
    m_buildingData.clear();
}

} // namespace Buildings
} // namespace Nova
