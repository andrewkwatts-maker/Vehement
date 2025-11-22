#include "FunctionBrowser.hpp"
#include "ScriptEditorPanel.hpp"
#include "../Editor.hpp"
#include "engine/scripting/ScriptStorage.hpp"
#include "engine/scripting/GameAPI.hpp"

#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

namespace Vehement {

FunctionBrowser::FunctionBrowser() = default;
FunctionBrowser::~FunctionBrowser() {
    Shutdown();
}

bool FunctionBrowser::Initialize(Editor* editor) {
    if (m_initialized) return true;

    m_editor = editor;

    // Discover all functions
    RefreshFunctions();

    m_initialized = true;
    return true;
}

void FunctionBrowser::Shutdown() {
    m_allFunctions.clear();
    m_filteredFunctions.clear();
    m_categoryTree.clear();
    m_selectedFunction = nullptr;
    m_initialized = false;
}

void FunctionBrowser::Render() {
    if (!m_initialized) return;

    if (ImGui::Begin("Function Browser", nullptr, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Preview", nullptr, &m_showPreview);
                ImGui::Separator();
                if (ImGui::MenuItem("Refresh", "F5")) {
                    RefreshFunctions();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Filter")) {
                ImGui::MenuItem("Game API", nullptr, &m_filter.showGameAPI);
                ImGui::MenuItem("Builtins", nullptr, &m_filter.showBuiltins);
                ImGui::MenuItem("Custom Scripts", nullptr, &m_filter.showCustom);
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Filter")) {
                    ClearFilter();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Toolbar
        RenderToolbar();

        // Search bar
        RenderSearchBar();

        // Main content
        float previewHeight = m_showPreview ? m_previewHeight : 0.0f;
        float treeHeight = ImGui::GetContentRegionAvail().y - previewHeight;

        // Tree view
        ImGui::BeginChild("TreeView", ImVec2(0, treeHeight), true);
        RenderTreeView();
        ImGui::EndChild();

        // Preview panel
        if (m_showPreview) {
            ImGui::BeginChild("Preview", ImVec2(0, 0), true);
            RenderPreviewPanel();
            ImGui::EndChild();
        }
    }
    ImGui::End();

    // Dialogs
    if (m_showNewFunctionDialog) {
        RenderNewFunctionDialog();
    }

    // Context menu
    RenderContextMenu();
}

void FunctionBrowser::Update(float deltaTime) {
    if (!m_initialized) return;

    // Auto-refresh
    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= REFRESH_INTERVAL) {
        RefreshFunctions();
        m_refreshTimer = 0.0f;
    }
}

void FunctionBrowser::RenderToolbar() {
    if (ImGui::Button("New Function")) {
        CreateNewFunction();
    }
    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        RefreshFunctions();
    }
    ImGui::SameLine();

    // Category filter dropdown
    ImGui::SetNextItemWidth(120);
    const char* categoryNames[] = { "All", "AI", "Combat", "Events", "Utility", "Entity", "Building", "Resource", "UI", "Audio", "Custom" };
    int currentCategory = static_cast<int>(m_filter.categoryFilter);

    if (ImGui::Combo("##CategoryFilter", &currentCategory, categoryNames, IM_ARRAYSIZE(categoryNames))) {
        m_filter.categoryFilter = static_cast<FunctionCategory>(currentCategory);
        ApplyFilter();
    }

    ImGui::Separator();
}

void FunctionBrowser::RenderSearchBar() {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Search", "Search functions...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_filter.searchText = m_searchBuffer;
        ApplyFilter();
    }
}

void FunctionBrowser::RenderTreeView() {
    if (m_filteredFunctions.empty()) {
        ImGui::TextDisabled("No functions found");
        return;
    }

    // Render category tree
    for (auto& category : m_categoryTree) {
        RenderCategoryNode(category);
    }
}

void FunctionBrowser::RenderCategoryNode(CategoryNode& node) {
    // Check if category has any matching functions
    bool hasMatchingFunctions = false;
    for (const auto& func : node.functions) {
        if (MatchesFilter(func)) {
            hasMatchingFunctions = true;
            break;
        }
    }

    if (!hasMatchingFunctions && node.children.empty()) {
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (node.expanded) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Category icon based on type
    const char* icon = "";
    switch (node.category) {
        case FunctionCategory::AI: icon = "[AI]"; break;
        case FunctionCategory::Combat: icon = "[Combat]"; break;
        case FunctionCategory::Events: icon = "[Events]"; break;
        case FunctionCategory::Utility: icon = "[Util]"; break;
        case FunctionCategory::Entity: icon = "[Entity]"; break;
        case FunctionCategory::Building: icon = "[Build]"; break;
        case FunctionCategory::Resource: icon = "[Res]"; break;
        case FunctionCategory::UI: icon = "[UI]"; break;
        case FunctionCategory::Audio: icon = "[Audio]"; break;
        default: icon = "[Custom]"; break;
    }

    std::string label = std::string(icon) + " " + node.name;

    if (ImGui::TreeNodeEx(label.c_str(), flags)) {
        node.expanded = true;

        // Render functions in this category
        for (auto& func : node.functions) {
            if (MatchesFilter(func)) {
                RenderFunctionItem(func);
            }
        }

        // Render child categories
        for (auto& child : node.children) {
            RenderCategoryNode(child);
        }

        ImGui::TreePop();
    } else {
        node.expanded = false;
    }
}

void FunctionBrowser::RenderFunctionItem(FunctionInfo& function) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool isSelected = (m_selectedFunction && m_selectedFunction->qualifiedName == function.qualifiedName);
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Color based on type
    ImVec4 color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    if (function.isGameAPI) {
        color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);  // Blue for game API
    } else if (function.isBuiltin) {
        color = ImVec4(0.8f, 0.8f, 0.6f, 1.0f);  // Yellow for builtins
    }

    ImGui::PushStyleColor(ImGuiCol_Text, color);

    // Function name with icon
    std::string label = function.name;
    if (function.isGameAPI) label = "@ " + label;
    else if (function.isBuiltin) label = "# " + label;
    else label = "- " + label;

    ImGui::TreeNodeEx(label.c_str(), flags);

    ImGui::PopStyleColor();

    // Handle selection
    if (ImGui::IsItemClicked()) {
        m_selectedFunction = &function;
        if (m_onSelectionChanged) {
            m_onSelectionChanged(function);
        }
    }

    // Handle double-click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (m_onDoubleClicked) {
            m_onDoubleClicked(function);
        }
        OpenInEditor(function);
    }

    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        m_isDragging = true;
        m_draggedFunction = &function;

        ImGui::SetDragDropPayload(GetDragDropPayloadType(), &function, sizeof(FunctionInfo*));
        ImGui::Text("Drag: %s", function.name.c_str());
        ImGui::EndDragDropSource();
    }

    // Tooltip with signature
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", function.signature.c_str());
        if (!function.description.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", function.description.c_str());
        }
        ImGui::EndTooltip();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open in Editor")) {
            OpenInEditor(function);
        }
        if (ImGui::MenuItem("Copy Name")) {
            ImGui::SetClipboardText(function.name.c_str());
        }
        if (ImGui::MenuItem("Copy Qualified Name")) {
            ImGui::SetClipboardText(function.qualifiedName.c_str());
        }
        if (ImGui::MenuItem("Copy Signature")) {
            ImGui::SetClipboardText(function.signature.c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", nullptr, false, !function.isBuiltin && !function.isGameAPI)) {
            m_selectedFunction = &function;
            DuplicateSelected();
        }
        if (ImGui::MenuItem("Delete", nullptr, false, !function.isBuiltin && !function.isGameAPI)) {
            m_selectedFunction = &function;
            DeleteSelected();
        }
        ImGui::EndPopup();
    }
}

void FunctionBrowser::RenderPreviewPanel() {
    if (!m_selectedFunction) {
        ImGui::TextDisabled("Select a function to preview");
        return;
    }

    RenderDocumentation(*m_selectedFunction);
}

void FunctionBrowser::RenderDocumentation(const FunctionInfo& function) {
    // Function name
    ImGui::PushFont(nullptr);  // Would use bold font
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", function.name.c_str());
    ImGui::PopFont();

    // Category badge
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", GetCategoryName(function.category));

    // Signature
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.6f, 1.0f), "%s", function.signature.c_str());

    ImGui::Separator();

    // Description
    if (!function.description.empty()) {
        ImGui::TextWrapped("%s", function.description.c_str());
        ImGui::Spacing();
    }

    // Parameters
    if (!function.parameters.empty()) {
        ImGui::Text("Parameters:");
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            std::string paramType = i < function.parameterTypes.size() ? function.parameterTypes[i] : "any";
            ImGui::BulletText("%s: %s", function.parameters[i].c_str(), paramType.c_str());
        }
        ImGui::Spacing();
    }

    // Return type
    if (!function.returnType.empty() && function.returnType != "None") {
        ImGui::Text("Returns: %s", function.returnType.c_str());
        ImGui::Spacing();
    }

    // Example code
    if (!function.exampleCode.empty()) {
        ImGui::Separator();
        ImGui::Text("Example:");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
        ImGui::BeginChild("Example", ImVec2(0, 80), true);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", function.exampleCode.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // File location
    if (!function.filePath.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("File: %s:%d", function.filePath.c_str(), function.lineNumber);
    }

    // Action buttons
    ImGui::Separator();
    if (ImGui::Button("Open in Editor")) {
        OpenInEditor(function);
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Signature")) {
        ImGui::SetClipboardText(function.signature.c_str());
    }
}

void FunctionBrowser::RenderNewFunctionDialog() {
    ImGui::OpenPopup("New Function");

    if (ImGui::BeginPopupModal("New Function", &m_showNewFunctionDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new Python function");
        ImGui::Separator();

        ImGui::InputText("Function Name", m_newFunctionName, sizeof(m_newFunctionName));

        // Category selection
        const char* categories[] = { "AI", "Combat", "Events", "Utility", "Entity", "Building", "Resource", "UI", "Audio", "Custom" };
        ImGui::Combo("Category", &m_newFunctionCategoryIndex, categories, IM_ARRAYSIZE(categories));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Would create new function file
            std::string funcName = m_newFunctionName;
            if (!funcName.empty()) {
                // Create function in script editor
                if (m_editor) {
                    auto* scriptEditor = m_editor->GetScriptEditor();
                    if (scriptEditor) {
                        // Would open with template
                    }
                }
            }
            m_showNewFunctionDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewFunctionDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FunctionBrowser::RenderContextMenu() {
    // Global context menu (right-click on empty space)
}

void FunctionBrowser::RefreshFunctions() {
    m_allFunctions.clear();
    m_functionIndex.clear();

    // Discover functions from all sources
    DiscoverGameAPIFunctions();
    DiscoverScriptFunctions();
    DiscoverBuiltinFunctions();

    // Build index
    for (size_t i = 0; i < m_allFunctions.size(); ++i) {
        m_functionIndex[m_allFunctions[i].qualifiedName] = i;
    }

    // Build category tree
    BuildCategoryTree();

    // Apply current filter
    ApplyFilter();
}

void FunctionBrowser::DiscoverGameAPIFunctions() {
    // Add game API functions
    std::vector<FunctionInfo> gameAPIFuncs = {
        // Entity functions
        {"spawn_entity", "nova.spawn_entity", "spawn_entity(type: str, x: float, y: float, z: float) -> int",
         "Spawn a new entity at the specified position",
         "Spawns an entity of the given type at world coordinates (x, y, z). Returns the entity ID.",
         "entity_id = spawn_entity('zombie', 10.0, 0.0, 15.0)",
         "", FunctionCategory::Entity, {"type", "x", "y", "z"}, {"str", "float", "float", "float"}, "int", true, false, 0},

        {"despawn_entity", "nova.despawn_entity", "despawn_entity(entity_id: int) -> None",
         "Remove an entity from the world",
         "Removes the entity with the given ID from the world.",
         "despawn_entity(enemy_id)",
         "", FunctionCategory::Entity, {"entity_id"}, {"int"}, "None", true, false, 0},

        {"get_position", "nova.get_position", "get_position(entity_id: int) -> Vec3",
         "Get entity position",
         "Returns the world position of the entity as a Vec3.",
         "pos = get_position(player_id)\nprint(f'Player at {pos.x}, {pos.y}, {pos.z}')",
         "", FunctionCategory::Entity, {"entity_id"}, {"int"}, "Vec3", true, false, 0},

        {"set_position", "nova.set_position", "set_position(entity_id: int, x: float, y: float, z: float) -> None",
         "Set entity position",
         "Teleports the entity to the specified world coordinates.",
         "set_position(player_id, 0.0, 0.0, 0.0)  # Move to origin",
         "", FunctionCategory::Entity, {"entity_id", "x", "y", "z"}, {"int", "float", "float", "float"}, "None", true, false, 0},

        // Combat functions
        {"damage", "nova.damage", "damage(target_id: int, amount: float, source_id: int = 0) -> None",
         "Apply damage to an entity",
         "Applies the specified amount of damage to the target. Optionally specify the source entity for attribution.",
         "damage(enemy_id, 50.0, player_id)",
         "", FunctionCategory::Combat, {"target_id", "amount", "source_id"}, {"int", "float", "int"}, "None", true, false, 0},

        {"heal", "nova.heal", "heal(target_id: int, amount: float) -> None",
         "Heal an entity",
         "Restores health to the target entity up to its maximum health.",
         "heal(ally_id, 25.0)",
         "", FunctionCategory::Combat, {"target_id", "amount"}, {"int", "float"}, "None", true, false, 0},

        {"get_health", "nova.get_health", "get_health(entity_id: int) -> float",
         "Get entity current health",
         "Returns the current health value of the entity.",
         "hp = get_health(player_id)\nif hp < 20:\n    show_warning('Low health!')",
         "", FunctionCategory::Combat, {"entity_id"}, {"int"}, "float", true, false, 0},

        {"is_alive", "nova.is_alive", "is_alive(entity_id: int) -> bool",
         "Check if entity is alive",
         "Returns True if the entity exists and has health > 0.",
         "if is_alive(target_id):\n    attack(target_id)",
         "", FunctionCategory::Combat, {"entity_id"}, {"int"}, "bool", true, false, 0},

        // Query functions
        {"find_entities_in_radius", "nova.find_entities_in_radius",
         "find_entities_in_radius(x: float, y: float, z: float, radius: float) -> List[int]",
         "Find all entities within radius",
         "Returns a list of entity IDs for all entities within the specified radius of the point.",
         "nearby = find_entities_in_radius(tower.x, tower.y, tower.z, 10.0)\nfor eid in nearby:\n    damage(eid, 5.0)",
         "", FunctionCategory::Utility, {"x", "y", "z", "radius"}, {"float", "float", "float", "float"}, "List[int]", true, false, 0},

        {"get_distance", "nova.get_distance", "get_distance(entity1: int, entity2: int) -> float",
         "Get distance between two entities",
         "Returns the Euclidean distance between two entities.",
         "dist = get_distance(player_id, enemy_id)\nif dist < 5.0:\n    attack(enemy_id)",
         "", FunctionCategory::Utility, {"entity1", "entity2"}, {"int", "int"}, "float", true, false, 0},

        // AI functions
        {"set_ai_target", "nova.set_ai_target", "set_ai_target(entity_id: int, target_id: int) -> None",
         "Set AI target",
         "Sets the target for the entity's AI behavior.",
         "set_ai_target(guard_id, intruder_id)",
         "", FunctionCategory::AI, {"entity_id", "target_id"}, {"int", "int"}, "None", true, false, 0},

        {"move_to", "nova.move_to", "move_to(entity_id: int, x: float, y: float, z: float) -> None",
         "Command entity to move to position",
         "Issues a movement command to the entity's AI.",
         "move_to(worker_id, resource.x, resource.y, resource.z)",
         "", FunctionCategory::AI, {"entity_id", "x", "y", "z"}, {"int", "float", "float", "float"}, "None", true, false, 0},

        // Audio/Visual
        {"play_sound", "nova.play_sound", "play_sound(name: str, x: float = 0, y: float = 0, z: float = 0) -> None",
         "Play a sound effect",
         "Plays the named sound at the specified position (or at listener if no position given).",
         "play_sound('explosion', enemy.x, enemy.y, enemy.z)",
         "", FunctionCategory::Audio, {"name", "x", "y", "z"}, {"str", "float", "float", "float"}, "None", true, false, 0},

        {"spawn_effect", "nova.spawn_effect", "spawn_effect(name: str, x: float, y: float, z: float) -> None",
         "Spawn a visual effect",
         "Spawns the named particle/visual effect at the position.",
         "spawn_effect('fire_explosion', target.x, target.y, target.z)",
         "", FunctionCategory::Audio, {"name", "x", "y", "z"}, {"str", "float", "float", "float"}, "None", true, false, 0},

        // UI functions
        {"show_notification", "nova.show_notification", "show_notification(message: str, duration: float = 3.0) -> None",
         "Show UI notification",
         "Displays a notification message to the player.",
         "show_notification('Quest completed!', 5.0)",
         "", FunctionCategory::UI, {"message", "duration"}, {"str", "float"}, "None", true, false, 0},

        // Time/Utility
        {"get_delta_time", "nova.get_delta_time", "get_delta_time() -> float",
         "Get frame delta time",
         "Returns the time elapsed since the last frame in seconds.",
         "dt = get_delta_time()\ntimer -= dt",
         "", FunctionCategory::Utility, {}, {}, "float", true, false, 0},

        {"get_game_time", "nova.get_game_time", "get_game_time() -> float",
         "Get total game time",
         "Returns the total elapsed game time in seconds.",
         "if get_game_time() > 300:  # 5 minutes\n    spawn_boss()",
         "", FunctionCategory::Utility, {}, {}, "float", true, false, 0},

        {"random", "nova.random", "random() -> float",
         "Get random float 0-1",
         "Returns a random float between 0.0 and 1.0.",
         "if random() < 0.1:  # 10% chance\n    drop_loot()",
         "", FunctionCategory::Utility, {}, {}, "float", true, false, 0},

        {"random_range", "nova.random_range", "random_range(min: float, max: float) -> float",
         "Get random float in range",
         "Returns a random float between min and max.",
         "damage_amount = random_range(10.0, 20.0)",
         "", FunctionCategory::Utility, {"min", "max"}, {"float", "float"}, "float", true, false, 0},

        // Debug
        {"log", "nova.log", "log(message: str) -> None",
         "Log message to console",
         "Writes an info message to the game console.",
         "log(f'Player health: {get_health(player_id)}')",
         "", FunctionCategory::Utility, {"message"}, {"str"}, "None", true, false, 0},
    };

    for (auto& func : gameAPIFuncs) {
        m_allFunctions.push_back(std::move(func));
    }
}

void FunctionBrowser::DiscoverScriptFunctions() {
    // Scan script directories for Python files
    std::vector<std::string> scriptPaths = {
        "game/assets/scripts/ai",
        "game/assets/scripts/events",
        "game/assets/scripts/pcg",
        "game/assets/scripts/examples"
    };

    for (const auto& path : scriptPaths) {
        if (std::filesystem::exists(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".py") {
                    ParsePythonFile(entry.path().string());
                }
            }
        }
    }
}

void FunctionBrowser::DiscoverBuiltinFunctions() {
    // Add common Python builtins that are useful
    std::vector<FunctionInfo> builtins = {
        {"print", "builtins.print", "print(*args, sep=' ', end='\\n') -> None",
         "Print to console", "", "print('Hello', name)", "", FunctionCategory::Utility,
         {"args"}, {"any"}, "None", false, true, 0},

        {"len", "builtins.len", "len(obj) -> int",
         "Return length of object", "", "count = len(enemies)", "", FunctionCategory::Utility,
         {"obj"}, {"Sized"}, "int", false, true, 0},

        {"range", "builtins.range", "range(stop) / range(start, stop[, step]) -> range",
         "Generate sequence of numbers", "", "for i in range(10):", "", FunctionCategory::Utility,
         {"start", "stop", "step"}, {"int", "int", "int"}, "range", false, true, 0},

        {"min", "builtins.min", "min(iterable) / min(a, b, ...) -> value",
         "Return minimum value", "", "lowest = min(scores)", "", FunctionCategory::Utility,
         {"values"}, {"Iterable"}, "any", false, true, 0},

        {"max", "builtins.max", "max(iterable) / max(a, b, ...) -> value",
         "Return maximum value", "", "highest = max(scores)", "", FunctionCategory::Utility,
         {"values"}, {"Iterable"}, "any", false, true, 0},

        {"abs", "builtins.abs", "abs(x) -> number",
         "Return absolute value", "", "distance = abs(a - b)", "", FunctionCategory::Utility,
         {"x"}, {"number"}, "number", false, true, 0},

        {"round", "builtins.round", "round(x[, ndigits]) -> number",
         "Round to given precision", "", "rounded = round(3.14159, 2)", "", FunctionCategory::Utility,
         {"x", "ndigits"}, {"float", "int"}, "number", false, true, 0},
    };

    for (auto& func : builtins) {
        m_allFunctions.push_back(std::move(func));
    }
}

void FunctionBrowser::ParsePythonFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Extract module name from file path
    std::string moduleName = filePath;
    size_t lastSlash = moduleName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        moduleName = moduleName.substr(lastSlash + 1);
    }
    size_t ext = moduleName.find(".py");
    if (ext != std::string::npos) {
        moduleName = moduleName.substr(0, ext);
    }

    // Parse function definitions
    std::regex funcRegex(R"(def\s+(\w+)\s*\(([^)]*)\)\s*(?:->\s*(\w+))?\s*:)");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    int lineNum = 1;

    while (std::regex_search(searchStart, content.cend(), match, funcRegex)) {
        // Count lines to this position
        for (auto it = searchStart; it != match[0].first; ++it) {
            if (*it == '\n') lineNum++;
        }

        FunctionInfo func;
        func.name = match[1].str();
        func.qualifiedName = moduleName + "." + func.name;
        func.filePath = filePath;
        func.lineNumber = lineNum;
        func.isGameAPI = false;
        func.isBuiltin = false;

        // Parse parameters
        std::string params = match[2].str();
        if (!params.empty()) {
            std::regex paramRegex(R"((\w+)(?:\s*:\s*(\w+))?(?:\s*=\s*[^,]+)?)");
            std::smatch paramMatch;
            std::string::const_iterator paramStart(params.cbegin());

            while (std::regex_search(paramStart, params.cend(), paramMatch, paramRegex)) {
                if (paramMatch[1].str() != "self") {
                    func.parameters.push_back(paramMatch[1].str());
                    func.parameterTypes.push_back(paramMatch[2].matched ? paramMatch[2].str() : "any");
                }
                paramStart = paramMatch.suffix().first;
            }
        }

        // Return type
        func.returnType = match[3].matched ? match[3].str() : "None";

        // Build signature
        std::string sig = func.name + "(";
        for (size_t i = 0; i < func.parameters.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += func.parameters[i];
            if (i < func.parameterTypes.size()) {
                sig += ": " + func.parameterTypes[i];
            }
        }
        sig += ") -> " + func.returnType;
        func.signature = sig;

        // Try to extract docstring
        size_t defEnd = match[0].second - content.cbegin();
        size_t docStart = content.find("\"\"\"", defEnd);
        if (docStart != std::string::npos && docStart - defEnd < 50) {
            size_t docEnd = content.find("\"\"\"", docStart + 3);
            if (docEnd != std::string::npos) {
                func.documentation = content.substr(docStart + 3, docEnd - docStart - 3);
                func.description = func.documentation.substr(0, func.documentation.find('\n'));
            }
        }

        // Categorize based on file path or function name
        if (filePath.find("/ai/") != std::string::npos || func.name.find("_ai") != std::string::npos) {
            func.category = FunctionCategory::AI;
        } else if (filePath.find("/events/") != std::string::npos || func.name.find("on_") == 0) {
            func.category = FunctionCategory::Events;
        } else if (filePath.find("/combat/") != std::string::npos) {
            func.category = FunctionCategory::Combat;
        } else {
            func.category = FunctionCategory::Custom;
        }

        m_allFunctions.push_back(std::move(func));

        searchStart = match.suffix().first;
    }
}

void FunctionBrowser::BuildCategoryTree() {
    m_categoryTree.clear();

    // Create category nodes
    std::unordered_map<FunctionCategory, CategoryNode*> categoryMap;

    const FunctionCategory categories[] = {
        FunctionCategory::AI, FunctionCategory::Combat, FunctionCategory::Events,
        FunctionCategory::Utility, FunctionCategory::Entity, FunctionCategory::Building,
        FunctionCategory::Resource, FunctionCategory::UI, FunctionCategory::Audio,
        FunctionCategory::Custom
    };

    for (auto cat : categories) {
        CategoryNode node;
        node.name = GetCategoryName(cat);
        node.category = cat;
        node.expanded = true;
        m_categoryTree.push_back(node);
        categoryMap[cat] = &m_categoryTree.back();
    }

    // Add functions to categories
    for (auto& func : m_allFunctions) {
        auto it = categoryMap.find(func.category);
        if (it != categoryMap.end()) {
            it->second->functions.push_back(func);
        }
    }

    // Sort functions within categories
    for (auto& node : m_categoryTree) {
        std::sort(node.functions.begin(), node.functions.end(),
                 [](const FunctionInfo& a, const FunctionInfo& b) {
                     return a.name < b.name;
                 });
    }
}

void FunctionBrowser::AddFunction(const FunctionInfo& function) {
    m_allFunctions.push_back(function);
    m_functionIndex[function.qualifiedName] = m_allFunctions.size() - 1;

    // Add to category tree
    AddToCategory(function);
    ApplyFilter();
}

void FunctionBrowser::RemoveFunction(const std::string& qualifiedName) {
    auto it = m_functionIndex.find(qualifiedName);
    if (it != m_functionIndex.end()) {
        m_allFunctions.erase(m_allFunctions.begin() + it->second);
        m_functionIndex.erase(it);

        // Rebuild tree
        BuildCategoryTree();
        ApplyFilter();
    }
}

void FunctionBrowser::AddToCategory(const FunctionInfo& function) {
    for (auto& node : m_categoryTree) {
        if (node.category == function.category) {
            node.functions.push_back(function);

            // Keep sorted
            std::sort(node.functions.begin(), node.functions.end(),
                     [](const FunctionInfo& a, const FunctionInfo& b) {
                         return a.name < b.name;
                     });
            break;
        }
    }
}

void FunctionBrowser::ApplyFilter() {
    m_filteredFunctions.clear();

    for (auto& func : m_allFunctions) {
        if (MatchesFilter(func)) {
            m_filteredFunctions.push_back(&func);
        }
    }
}

bool FunctionBrowser::MatchesFilter(const FunctionInfo& function) const {
    // Category filter
    if (m_filter.categoryFilter != FunctionCategory::All &&
        function.category != m_filter.categoryFilter) {
        return false;
    }

    // Type filters
    if (!m_filter.showGameAPI && function.isGameAPI) return false;
    if (!m_filter.showBuiltins && function.isBuiltin) return false;
    if (!m_filter.showCustom && !function.isGameAPI && !function.isBuiltin) return false;

    // Text search
    if (!m_filter.searchText.empty()) {
        std::string searchLower = m_filter.searchText;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        std::string nameLower = function.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        std::string descLower = function.description;
        std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);

        if (nameLower.find(searchLower) == std::string::npos &&
            descLower.find(searchLower) == std::string::npos) {
            return false;
        }
    }

    return true;
}

void FunctionBrowser::SetFilter(const FunctionSearchFilter& filter) {
    m_filter = filter;
    strncpy(m_searchBuffer, filter.searchText.c_str(), sizeof(m_searchBuffer) - 1);
    ApplyFilter();
}

void FunctionBrowser::ClearFilter() {
    m_filter = FunctionSearchFilter();
    m_searchBuffer[0] = '\0';
    ApplyFilter();
}

std::vector<FunctionInfo> FunctionBrowser::GetFunctionsByCategory(FunctionCategory category) const {
    std::vector<FunctionInfo> result;
    for (const auto& func : m_allFunctions) {
        if (func.category == category) {
            result.push_back(func);
        }
    }
    return result;
}

std::optional<FunctionInfo> FunctionBrowser::FindFunction(const std::string& qualifiedName) const {
    auto it = m_functionIndex.find(qualifiedName);
    if (it != m_functionIndex.end()) {
        return m_allFunctions[it->second];
    }
    return std::nullopt;
}

void FunctionBrowser::SelectFunction(const std::string& qualifiedName) {
    auto func = FindFunction(qualifiedName);
    if (func) {
        for (auto& f : m_allFunctions) {
            if (f.qualifiedName == qualifiedName) {
                m_selectedFunction = &f;
                if (m_onSelectionChanged) {
                    m_onSelectionChanged(f);
                }
                break;
            }
        }
    }
}

void FunctionBrowser::ClearSelection() {
    m_selectedFunction = nullptr;
}

void FunctionBrowser::BeginDrag(const FunctionInfo* function) {
    m_isDragging = true;
    m_draggedFunction = function;
}

void FunctionBrowser::EndDrag() {
    if (m_isDragging && m_draggedFunction && m_onFunctionDropped) {
        m_onFunctionDropped(*m_draggedFunction);
    }
    m_isDragging = false;
    m_draggedFunction = nullptr;
}

void FunctionBrowser::OpenInEditor(const FunctionInfo& function) {
    if (function.filePath.empty()) {
        // For game API functions, show documentation or generate stub
        return;
    }

    if (m_editor) {
        // Would open the file in script editor at the line number
    }
}

void FunctionBrowser::CreateNewFunction() {
    m_showNewFunctionDialog = true;
    m_newFunctionName[0] = '\0';
    m_newFunctionCategoryIndex = static_cast<int>(FunctionCategory::Custom) - 1;
}

void FunctionBrowser::DuplicateSelected() {
    if (!m_selectedFunction) return;

    FunctionInfo copy = *m_selectedFunction;
    copy.name = copy.name + "_copy";
    copy.qualifiedName = copy.qualifiedName + "_copy";

    AddFunction(copy);
}

void FunctionBrowser::DeleteSelected() {
    if (!m_selectedFunction) return;

    // Can't delete builtins or game API
    if (m_selectedFunction->isBuiltin || m_selectedFunction->isGameAPI) {
        return;
    }

    RemoveFunction(m_selectedFunction->qualifiedName);
    m_selectedFunction = nullptr;
}

const char* FunctionBrowser::GetCategoryName(FunctionCategory category) {
    switch (category) {
        case FunctionCategory::AI: return "AI";
        case FunctionCategory::Combat: return "Combat";
        case FunctionCategory::Events: return "Events";
        case FunctionCategory::Utility: return "Utility";
        case FunctionCategory::Entity: return "Entity";
        case FunctionCategory::Building: return "Building";
        case FunctionCategory::Resource: return "Resource";
        case FunctionCategory::UI: return "UI";
        case FunctionCategory::Audio: return "Audio";
        case FunctionCategory::Custom: return "Custom";
        case FunctionCategory::All: return "All";
        default: return "Unknown";
    }
}

FunctionCategory FunctionBrowser::ParseCategory(const std::string& name) {
    if (name == "AI") return FunctionCategory::AI;
    if (name == "Combat") return FunctionCategory::Combat;
    if (name == "Events") return FunctionCategory::Events;
    if (name == "Utility") return FunctionCategory::Utility;
    if (name == "Entity") return FunctionCategory::Entity;
    if (name == "Building") return FunctionCategory::Building;
    if (name == "Resource") return FunctionCategory::Resource;
    if (name == "UI") return FunctionCategory::UI;
    if (name == "Audio") return FunctionCategory::Audio;
    return FunctionCategory::Custom;
}

} // namespace Vehement
