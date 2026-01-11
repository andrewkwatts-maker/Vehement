/**
 * @file InstanceSystem_IntegrationExample.cpp
 * @brief Example integration of Instance Property Editor with StandaloneEditor
 *
 * This file demonstrates how to integrate the instance-specific property editing
 * system with the StandaloneEditor. Add these modifications to your StandaloneEditor
 * class to enable full instance property editing functionality.
 */

#include "StandaloneEditor.hpp"
#include "InstancePropertyEditor.hpp"
#include "scene/InstanceManager.hpp"
#include "scene/InstanceData.hpp"

/**
 * STEP 1: Add these member variables to StandaloneEditor class (in .hpp)
 * Add to the private section, near other managers:
 */
/*
    // Instance management
    std::unique_ptr<Nova::InstanceManager> m_instanceManager;
    std::unique_ptr<InstancePropertyEditor> m_propertyEditor;

    // Map name for saving instances
    std::string m_currentMapName = "test_map";
*/

/**
 * STEP 2: Initialize in StandaloneEditor::Initialize()
 */
void ExampleInitialize() {
    // ... existing initialization code ...

    // Initialize instance manager
    m_instanceManager = std::make_unique<Nova::InstanceManager>();
    if (!m_instanceManager->Initialize("assets/config/", "assets/maps/")) {
        spdlog::error("Failed to initialize InstanceManager");
    }

    // Initialize property editor
    m_propertyEditor = std::make_unique<InstancePropertyEditor>();
    m_propertyEditor->Initialize(m_instanceManager.get());
    m_propertyEditor->SetAutoSaveEnabled(true);
    m_propertyEditor->SetAutoSaveDelay(2.0f);  // Auto-save after 2 seconds

    // Load instances for current map
    auto instances = m_instanceManager->LoadMapInstances(m_currentMapName);
    spdlog::info("Loaded {} instances for map: {}", instances.size(), m_currentMapName);

    // ... rest of initialization ...
}

/**
 * STEP 3: Update in StandaloneEditor::Update()
 */
void ExampleUpdate(float deltaTime) {
    // ... existing update code ...

    // Update property editor (handles auto-save)
    if (m_propertyEditor) {
        m_propertyEditor->Update(deltaTime);
    }

    // ... rest of update code ...
}

/**
 * STEP 4: Replace RenderDetailsPanel() implementation
 */
void StandaloneEditor::RenderDetailsPanel() {
    ImGui::Begin("Details");

    if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size())) {
        auto& selectedObject = m_sceneObjects[m_selectedObjectIndex];

        // If object doesn't have an instance ID, create one
        if (selectedObject.instanceId.empty() && !selectedObject.archetypeId.empty()) {
            Nova::InstanceData instance = m_instanceManager->CreateInstance(
                selectedObject.archetypeId,
                selectedObject.position
            );
            selectedObject.instanceId = instance.instanceId;

            // Initialize transform
            instance.rotation = glm::quat(glm::radians(selectedObject.rotation));
            instance.scale = selectedObject.scale;

            m_instanceManager->RegisterInstance(instance);
        }

        // Render property editor
        if (m_propertyEditor && !selectedObject.instanceId.empty()) {
            m_propertyEditor->RenderPanel(
                selectedObject.instanceId,
                m_selectedObjectPosition,
                m_selectedObjectRotation,
                m_selectedObjectScale
            );

            // Sync back to scene object
            selectedObject.position = m_selectedObjectPosition;
            selectedObject.rotation = m_selectedObjectRotation;
            selectedObject.scale = m_selectedObjectScale;
        } else {
            // Fallback to basic property display
            ImGui::Text("Selected Object: %s", selectedObject.name.c_str());
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Position", &m_selectedObjectPosition.x, 0.1f);
                ImGui::DragFloat3("Rotation", &m_selectedObjectRotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &m_selectedObjectScale.x, 0.01f);
            }
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
        ImGui::Separator();

        // Show scene settings when nothing is selected
        ImGui::Text("Scene Settings");
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float ambientIntensity = 0.2f;
            ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
        }
    }

    ImGui::End();
}

/**
 * STEP 5: Add PlaceObject override to create instances
 */
void ExamplePlaceObject(const glm::vec3& position, const std::string& archetypeId) {
    // Create instance
    Nova::InstanceData instance = m_instanceManager->CreateInstance(archetypeId, position);

    // Create scene object
    SceneObject obj;
    obj.name = instance.name;
    obj.position = position;
    obj.rotation = glm::vec3(0.0f);
    obj.scale = glm::vec3(1.0f);
    obj.instanceId = instance.instanceId;
    obj.archetypeId = archetypeId;

    // Load archetype to get bounding box info
    nlohmann::json archetype = m_instanceManager->LoadArchetype(archetypeId);
    if (archetype.contains("model") && archetype["model"].contains("scale")) {
        float scale = archetype["model"]["scale"].get<float>();
        obj.boundingBoxMin = glm::vec3(-0.5f * scale);
        obj.boundingBoxMax = glm::vec3(0.5f * scale);
    }

    m_sceneObjects.push_back(obj);

    spdlog::info("Placed object: {} (archetype: {})", instance.name, archetypeId);
}

/**
 * STEP 6: Save instances when saving map
 */
void ExampleSaveMap(const std::string& path) {
    // ... existing map save code ...

    // Save all instances
    if (m_propertyEditor) {
        m_propertyEditor->SaveAll(m_currentMapName);
    }

    // Or save dirty instances only
    if (m_instanceManager) {
        int savedCount = m_instanceManager->SaveDirtyInstances(m_currentMapName);
        spdlog::info("Saved {} instances with map", savedCount);
    }

    // ... rest of save code ...
}

/**
 * STEP 7: Load instances when loading map
 */
void ExampleLoadMap(const std::string& path) {
    // ... existing map load code ...

    // Load instances for this map
    if (m_instanceManager) {
        auto instances = m_instanceManager->LoadMapInstances(m_currentMapName);

        // Create scene objects from instances
        for (const auto& instance : instances) {
            SceneObject obj;
            obj.name = instance.name;
            obj.position = instance.position;
            obj.rotation = glm::degrees(glm::eulerAngles(instance.rotation));
            obj.scale = instance.scale;
            obj.instanceId = instance.instanceId;
            obj.archetypeId = instance.archetypeId;

            // Load archetype for additional data
            nlohmann::json archetype = m_instanceManager->LoadArchetype(instance.archetypeId);
            // ... set bounding box, mesh, etc. from archetype ...

            m_sceneObjects.push_back(obj);
        }

        spdlog::info("Loaded {} instances from map", instances.size());
    }

    // ... rest of load code ...
}

/**
 * STEP 8: Add menu items for instance management
 */
void ExampleRenderMenuBar() {
    // ... existing menu code ...

    if (ImGui::BeginMenu("Instances")) {
        if (ImGui::MenuItem("Save All Instances", "Ctrl+Shift+S")) {
            if (m_propertyEditor) {
                m_propertyEditor->SaveAll(m_currentMapName);
            }
        }

        ImGui::Separator();

        if (m_propertyEditor) {
            int dirtyCount = m_propertyEditor->GetDirtyCount();
            if (dirtyCount > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                                   "%d unsaved instance(s)", dirtyCount);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                                   "All instances saved");
            }
        }

        ImGui::Separator();

        // List available archetypes
        if (ImGui::BeginMenu("Place Archetype")) {
            auto archetypes = m_instanceManager->ListArchetypes();
            for (const auto& archetypeId : archetypes) {
                if (ImGui::MenuItem(archetypeId.c_str())) {
                    // Place at camera position or cursor
                    ExamplePlaceObject(m_editorCameraTarget, archetypeId);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    // ... rest of menu code ...
}

/**
 * USAGE EXAMPLE:
 *
 * 1. Place an object in the scene:
 *    - User selects "humans.units.footman" from asset browser
 *    - Clicks in viewport to place
 *    - ExamplePlaceObject() creates instance and scene object
 *
 * 2. Select the object:
 *    - User clicks on footman in viewport
 *    - m_selectedObjectIndex is set
 *    - RenderDetailsPanel() shows property editor
 *
 * 3. Edit properties:
 *    - User sees "Archetype Properties" (read-only, gray)
 *    - Clicks "Override" next to "stats.health"
 *    - Property moves to "Instance Overrides" (editable, white)
 *    - User changes health from 100 to 150
 *    - Property is marked dirty
 *
 * 4. Auto-save:
 *    - After 2 seconds of no changes, auto-save triggers
 *    - Instance JSON saved to: assets/maps/test_map/instances/{instanceId}.json
 *
 * 5. Custom data:
 *    - User adds custom property: "quest_giver" = true
 *    - Adds custom property: "dialog_id" = "quest_001"
 *    - These are saved in the "customData" section
 *
 * 6. Load map:
 *    - Map loads, reads all instances/*.json files
 *    - Scene objects created with proper overrides applied
 *    - Captain Marcus appears with 150 health instead of 100
 */
