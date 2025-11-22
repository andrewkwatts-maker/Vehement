#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../../rts/world/PortalGate.hpp"

namespace Vehement {

/**
 * @brief Portal UI state
 */
enum class PortalUIState : uint8_t {
    Closed,
    PortalInfo,
    DestinationPreview,
    ArmySelection,
    ResourceSelection,
    Confirmation,
    Traveling,
    Arrived
};

/**
 * @brief Unit selection for travel
 */
struct TravelUnitSelection {
    std::string unitId;
    std::string unitType;
    std::string unitName;
    int count = 1;
    bool selected = false;
    bool canTravel = true;
    std::string restrictionReason;
};

/**
 * @brief Resource selection for transfer
 */
struct TravelResourceSelection {
    std::string resourceType;
    std::string displayName;
    int available = 0;
    int selected = 0;
    int maxTransfer = 0;
    std::string iconPath;
};

/**
 * @brief Portal travel preview
 */
struct TravelPreview {
    std::string sourceRegionId;
    std::string sourceRegionName;
    std::string destinationRegionId;
    std::string destinationRegionName;
    float estimatedTravelTime = 0.0f;
    std::unordered_map<std::string, int> cost;
    std::vector<std::string> warnings;
    bool canTravel = true;
    std::string blockReason;
    float encounterChance = 0.0f;
    int dangerLevel = 1;
    std::string destinationBiome;
    std::string destinationWeather;
    int playersAtDestination = 0;
};

/**
 * @brief Portal UI configuration
 */
struct PortalUIConfig {
    bool showDestinationPreview = true;
    bool showTravelCost = true;
    bool showWarnings = true;
    bool showEncounterChance = true;
    bool allowPartialSelection = true;
    float previewUpdateInterval = 5.0f;
};

/**
 * @brief Portal interaction UI
 */
class PortalUI {
public:
    using TravelConfirmCallback = std::function<void(const std::vector<std::string>& unitIds,
                                                      const std::unordered_map<std::string, int>& resources)>;
    using TravelCancelCallback = std::function<void()>;
    using PortalCloseCallback = std::function<void()>;

    [[nodiscard]] static PortalUI& Instance();

    PortalUI(const PortalUI&) = delete;
    PortalUI& operator=(const PortalUI&) = delete;

    [[nodiscard]] bool Initialize(const PortalUIConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);
    void Render();

    // ==================== Portal Display ====================

    /**
     * @brief Open portal UI for specific portal
     */
    void OpenPortal(const std::string& portalId);

    /**
     * @brief Close portal UI
     */
    void Close();

    /**
     * @brief Check if UI is open
     */
    [[nodiscard]] bool IsOpen() const { return m_state != PortalUIState::Closed; }

    /**
     * @brief Get current portal ID
     */
    [[nodiscard]] std::string GetCurrentPortalId() const { return m_currentPortalId; }

    /**
     * @brief Get current state
     */
    [[nodiscard]] PortalUIState GetState() const { return m_state; }

    // ==================== Navigation ====================

    void ShowPortalInfo();
    void ShowDestinationPreview();
    void ShowArmySelection();
    void ShowResourceSelection();
    void ShowConfirmation();

    void NextStep();
    void PreviousStep();

    // ==================== Unit Selection ====================

    void SetAvailableUnits(const std::vector<TravelUnitSelection>& units);
    void SelectUnit(const std::string& unitId, bool selected);
    void SelectAllUnits();
    void DeselectAllUnits();
    [[nodiscard]] std::vector<std::string> GetSelectedUnitIds() const;
    [[nodiscard]] int GetSelectedUnitCount() const;

    // ==================== Resource Selection ====================

    void SetAvailableResources(const std::vector<TravelResourceSelection>& resources);
    void SetResourceAmount(const std::string& resourceType, int amount);
    void SetMaxResources();
    void ClearResources();
    [[nodiscard]] std::unordered_map<std::string, int> GetSelectedResources() const;

    // ==================== Travel Preview ====================

    [[nodiscard]] TravelPreview GetTravelPreview() const;
    void RefreshPreview();

    // ==================== Actions ====================

    void ConfirmTravel();
    void CancelTravel();

    // ==================== Travel Progress ====================

    void SetTravelProgress(float progress);
    [[nodiscard]] float GetTravelProgress() const { return m_travelProgress; }

    void SetTravelState(TravelState state);
    void ShowArrival(const std::string& regionName);

    // ==================== Requirements Display ====================

    [[nodiscard]] bool CheckRequirements() const;
    [[nodiscard]] std::vector<std::string> GetUnmetRequirements() const;

    // ==================== Callbacks ====================

    void OnTravelConfirmed(TravelConfirmCallback callback);
    void OnTravelCancelled(TravelCancelCallback callback);
    void OnClosed(PortalCloseCallback callback);

    // ==================== JS Bridge ====================

    [[nodiscard]] nlohmann::json GetPortalDataForJS() const;
    void HandleJSCommand(const std::string& command, const nlohmann::json& params);

    // ==================== Configuration ====================

    [[nodiscard]] const PortalUIConfig& GetConfig() const { return m_config; }

private:
    PortalUI() = default;
    ~PortalUI() = default;

    void UpdatePreview();
    void ValidateSelection();

    bool m_initialized = false;
    PortalUIConfig m_config;
    PortalUIState m_state = PortalUIState::Closed;

    std::string m_currentPortalId;
    std::string m_destinationRegionId;

    // Units
    std::vector<TravelUnitSelection> m_availableUnits;
    std::unordered_set<std::string> m_selectedUnitIds;

    // Resources
    std::vector<TravelResourceSelection> m_availableResources;
    std::unordered_map<std::string, int> m_selectedResources;

    // Preview
    TravelPreview m_preview;
    float m_previewTimer = 0.0f;

    // Travel progress
    float m_travelProgress = 0.0f;
    TravelState m_travelState = TravelState::Idle;

    // Callbacks
    std::vector<TravelConfirmCallback> m_confirmCallbacks;
    std::vector<TravelCancelCallback> m_cancelCallbacks;
    std::vector<PortalCloseCallback> m_closeCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
