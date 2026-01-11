#include "GameAPI.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Nova {
namespace Scripting {

GameAPI::GameAPI() = default;
GameAPI::~GameAPI() {
    Shutdown();
}

bool GameAPI::Initialize() {
    if (m_initialized) return true;

    RegisterBuiltinAPI();
    RegisterBuiltinTypes();
    RegisterBuiltinEvents();

    m_initialized = true;
    return true;
}

void GameAPI::Shutdown() {
    m_functions.clear();
    m_types.clear();
    m_events.clear();
    m_functionIndex.clear();
    m_typeIndex.clear();
    m_eventIndex.clear();
    m_cachedCompletions.clear();
    m_initialized = false;
}

void GameAPI::RegisterFunction(const APIFunctionDoc& func) {
    auto it = m_functionIndex.find(func.name);
    if (it != m_functionIndex.end()) {
        m_functions[it->second] = func;
    } else {
        m_functions.push_back(func);
        m_functionIndex[func.name] = m_functions.size() - 1;
    }
    m_completionsCacheDirty = true;
}

std::optional<APIFunctionDoc> GameAPI::GetFunction(const std::string& name) const {
    auto it = m_functionIndex.find(name);
    if (it != m_functionIndex.end()) {
        return m_functions[it->second];
    }
    return std::nullopt;
}

std::vector<APIFunctionDoc> GameAPI::GetFunctionsByCategory(const std::string& category) const {
    std::vector<APIFunctionDoc> result;
    for (const auto& func : m_functions) {
        if (func.category == category) {
            result.push_back(func);
        }
    }
    return result;
}

std::vector<std::string> GameAPI::GetFunctionCategories() const {
    std::vector<std::string> categories;
    for (const auto& func : m_functions) {
        if (std::find(categories.begin(), categories.end(), func.category) == categories.end()) {
            categories.push_back(func.category);
        }
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

void GameAPI::RegisterType(const APITypeDoc& type) {
    auto it = m_typeIndex.find(type.name);
    if (it != m_typeIndex.end()) {
        m_types[it->second] = type;
    } else {
        m_types.push_back(type);
        m_typeIndex[type.name] = m_types.size() - 1;
    }
    m_completionsCacheDirty = true;
}

std::optional<APITypeDoc> GameAPI::GetType(const std::string& name) const {
    auto it = m_typeIndex.find(name);
    if (it != m_typeIndex.end()) {
        return m_types[it->second];
    }
    return std::nullopt;
}

void GameAPI::RegisterEvent(const APIEventDoc& event) {
    auto it = m_eventIndex.find(event.name);
    if (it != m_eventIndex.end()) {
        m_events[it->second] = event;
    } else {
        m_events.push_back(event);
        m_eventIndex[event.name] = m_events.size() - 1;
    }
    m_completionsCacheDirty = true;
}

std::optional<APIEventDoc> GameAPI::GetEvent(const std::string& name) const {
    auto it = m_eventIndex.find(name);
    if (it != m_eventIndex.end()) {
        return m_events[it->second];
    }
    return std::nullopt;
}

std::vector<AutoCompleteItem> GameAPI::GetCompletions(const std::string& prefix) const {
    std::vector<AutoCompleteItem> result;
    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

    auto allCompletions = GetAllCompletions();

    for (const auto& item : allCompletions) {
        std::string lowerText = item.text;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

        if (lowerText.find(lowerPrefix) == 0) {
            result.push_back(item);
        }
    }

    // Sort by relevance
    std::sort(result.begin(), result.end(), [](const AutoCompleteItem& a, const AutoCompleteItem& b) {
        return a.sortOrder < b.sortOrder;
    });

    return result;
}

std::vector<AutoCompleteItem> GameAPI::GetCompletionsForContext(
    const std::string& context, const std::string& prefix) const {

    std::vector<AutoCompleteItem> result;

    // Check if context is a known type
    auto typeIt = m_typeIndex.find(context);
    if (typeIt != m_typeIndex.end()) {
        const auto& type = m_types[typeIt->second];

        // Add methods
        for (const auto& method : type.methods) {
            auto item = FunctionToCompletion(method);
            if (prefix.empty() || item.text.find(prefix) == 0) {
                result.push_back(item);
            }
        }

        // Add properties
        for (const auto& prop : type.properties) {
            AutoCompleteItem item;
            item.text = prop.name;
            item.displayText = prop.name + ": " + prop.type;
            item.insertText = prop.name;
            item.detail = prop.type;
            item.documentation = prop.description;
            item.kind = "property";
            if (prefix.empty() || item.text.find(prefix) == 0) {
                result.push_back(item);
            }
        }
    }

    return result;
}

std::vector<AutoCompleteItem> GameAPI::GetAllCompletions() const {
    if (!m_completionsCacheDirty) {
        return m_cachedCompletions;
    }

    m_cachedCompletions.clear();

    // Add functions
    for (const auto& func : m_functions) {
        m_cachedCompletions.push_back(FunctionToCompletion(func));
    }

    // Add types
    for (const auto& type : m_types) {
        m_cachedCompletions.push_back(TypeToCompletion(type));
    }

    m_completionsCacheDirty = false;
    return m_cachedCompletions;
}

bool GameAPI::ExportCompletions(const std::string& filePath) const {
    json j = json::array();

    for (const auto& item : GetAllCompletions()) {
        json itemJson;
        itemJson["text"] = item.text;
        itemJson["displayText"] = item.displayText;
        itemJson["insertText"] = item.insertText;
        itemJson["detail"] = item.detail;
        itemJson["documentation"] = item.documentation;
        itemJson["kind"] = item.kind;
        j.push_back(itemJson);
    }

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return file.good();
}

bool GameAPI::GenerateTypeStubs(const std::string& filePath) const {
    std::stringstream ss;

    ss << "\"\"\"" << "\n";
    ss << "Nova Game Engine Python API Type Stubs" << "\n";
    ss << "Auto-generated - Do not edit manually" << "\n";
    ss << "\"\"\"" << "\n\n";

    ss << "from typing import List, Optional, Tuple, Union, Any\n\n";

    // Generate type stubs
    for (const auto& type : m_types) {
        ss << GenerateStubForType(type) << "\n\n";
    }

    ss << "# Functions\n\n";

    // Group by category
    auto categories = GetFunctionCategories();
    for (const auto& category : categories) {
        ss << "# " << category << "\n";
        auto funcs = GetFunctionsByCategory(category);
        for (const auto& func : funcs) {
            ss << GenerateStubForFunction(func) << "\n";
        }
        ss << "\n";
    }

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << ss.str();
    return file.good();
}

bool GameAPI::GenerateHTMLDocs(const std::string& outputDir) const {
    // Generate index.html
    std::stringstream indexHtml;
    indexHtml << "<!DOCTYPE html>\n<html>\n<head>\n";
    indexHtml << "<title>Nova Game API Documentation</title>\n";
    indexHtml << "<style>\n";
    indexHtml << "body { font-family: sans-serif; margin: 40px; }\n";
    indexHtml << ".function { margin: 20px 0; padding: 15px; background: #f5f5f5; }\n";
    indexHtml << ".signature { font-family: monospace; color: #0066cc; }\n";
    indexHtml << ".description { margin: 10px 0; }\n";
    indexHtml << ".params { margin-left: 20px; }\n";
    indexHtml << "</style>\n</head>\n<body>\n";
    indexHtml << "<h1>Nova Game API</h1>\n";

    auto categories = GetFunctionCategories();
    for (const auto& category : categories) {
        indexHtml << "<h2>" << category << "</h2>\n";
        auto funcs = GetFunctionsByCategory(category);
        for (const auto& func : funcs) {
            indexHtml << "<div class='function'>\n";
            indexHtml << "<div class='signature'>" << func.signature << "</div>\n";
            indexHtml << "<div class='description'>" << func.description << "</div>\n";
            if (!func.parameters.empty()) {
                indexHtml << "<div class='params'><strong>Parameters:</strong><ul>\n";
                for (const auto& param : func.parameters) {
                    indexHtml << "<li><code>" << param.name << "</code> (" << param.type << "): ";
                    indexHtml << param.description << "</li>\n";
                }
                indexHtml << "</ul></div>\n";
            }
            if (!func.returnType.empty() && func.returnType != "None") {
                indexHtml << "<div><strong>Returns:</strong> " << func.returnType;
                if (!func.returnDescription.empty()) {
                    indexHtml << " - " << func.returnDescription;
                }
                indexHtml << "</div>\n";
            }
            indexHtml << "</div>\n";
        }
    }

    indexHtml << "</body>\n</html>";

    std::ofstream file(outputDir + "/index.html");
    if (!file.is_open()) return false;

    file << indexHtml.str();
    return file.good();
}

bool GameAPI::GenerateMarkdownDocs(const std::string& filePath) const {
    std::stringstream ss;

    ss << "# Nova Game API Reference\n\n";

    auto categories = GetFunctionCategories();
    for (const auto& category : categories) {
        ss << "## " << category << "\n\n";
        auto funcs = GetFunctionsByCategory(category);
        for (const auto& func : funcs) {
            ss << "### `" << func.name << "`\n\n";
            ss << "```python\n" << func.signature << "\n```\n\n";
            ss << func.description << "\n\n";

            if (!func.parameters.empty()) {
                ss << "**Parameters:**\n";
                for (const auto& param : func.parameters) {
                    ss << "- `" << param.name << "` (" << param.type << "): " << param.description << "\n";
                }
                ss << "\n";
            }

            if (!func.returnType.empty() && func.returnType != "None") {
                ss << "**Returns:** `" << func.returnType << "`";
                if (!func.returnDescription.empty()) {
                    ss << " - " << func.returnDescription;
                }
                ss << "\n\n";
            }

            if (!func.example.empty()) {
                ss << "**Example:**\n```python\n" << func.example << "\n```\n\n";
            }
        }
    }

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << ss.str();
    return file.good();
}

std::string GameAPI::GetDocumentationJSON() const {
    json j;

    j["functions"] = json::array();
    for (const auto& func : m_functions) {
        json f;
        f["name"] = func.name;
        f["signature"] = func.signature;
        f["description"] = func.description;
        f["category"] = func.category;
        f["returnType"] = func.returnType;
        f["example"] = func.example;

        f["parameters"] = json::array();
        for (const auto& param : func.parameters) {
            json p;
            p["name"] = param.name;
            p["type"] = param.type;
            p["description"] = param.description;
            p["optional"] = param.optional;
            f["parameters"].push_back(p);
        }
        j["functions"].push_back(f);
    }

    j["types"] = json::array();
    for (const auto& type : m_types) {
        json t;
        t["name"] = type.name;
        t["description"] = type.description;
        t["category"] = type.category;
        j["types"].push_back(t);
    }

    j["events"] = json::array();
    for (const auto& event : m_events) {
        json e;
        e["name"] = event.name;
        e["description"] = event.description;
        j["events"].push_back(e);
    }

    return j.dump(2);
}

std::vector<GameAPI::SearchResult> GameAPI::Search(const std::string& query) const {
    std::vector<SearchResult> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& func : m_functions) {
        std::string lowerName = func.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string lowerDesc = func.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        float relevance = 0.0f;
        if (lowerName.find(lowerQuery) == 0) relevance = 1.0f;
        else if (lowerName.find(lowerQuery) != std::string::npos) relevance = 0.8f;
        else if (lowerDesc.find(lowerQuery) != std::string::npos) relevance = 0.5f;

        if (relevance > 0) {
            results.push_back({SearchResult::Type::Function, func.name, func.description, func.signature, relevance});
        }
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.relevance > b.relevance;
    });

    return results;
}

AutoCompleteItem GameAPI::FunctionToCompletion(const APIFunctionDoc& func) const {
    AutoCompleteItem item;
    item.text = func.name;
    item.displayText = func.name + "()";
    item.insertText = func.name + "(";
    item.detail = func.signature;
    item.documentation = func.description;
    item.kind = "function";
    item.sortOrder = func.deprecated ? 100 : 0;
    return item;
}

AutoCompleteItem GameAPI::TypeToCompletion(const APITypeDoc& type) const {
    AutoCompleteItem item;
    item.text = type.name;
    item.displayText = type.name;
    item.insertText = type.name;
    item.detail = "class " + type.name;
    item.documentation = type.description;
    item.kind = "class";
    return item;
}

std::string GameAPI::GenerateStubForFunction(const APIFunctionDoc& func) const {
    std::stringstream ss;

    ss << "def " << func.name << "(";

    for (size_t i = 0; i < func.parameters.size(); ++i) {
        if (i > 0) ss << ", ";
        const auto& param = func.parameters[i];
        ss << param.name << ": " << param.type;
        if (param.optional && !param.defaultValue.empty()) {
            ss << " = " << param.defaultValue;
        }
    }

    ss << ") -> " << (func.returnType.empty() ? "None" : func.returnType) << ":\n";
    ss << "    \"\"\"" << func.description << "\"\"\"\n";
    ss << "    ...\n";

    return ss.str();
}

std::string GameAPI::GenerateStubForType(const APITypeDoc& type) const {
    std::stringstream ss;

    ss << "class " << type.name << ":\n";
    ss << "    \"\"\"" << type.description << "\"\"\"\n";

    for (const auto& prop : type.properties) {
        ss << "    " << prop.name << ": " << prop.type << "\n";
    }

    for (const auto& method : type.methods) {
        ss << "    " << GenerateStubForFunction(method);
    }

    if (type.properties.empty() && type.methods.empty()) {
        ss << "    pass\n";
    }

    return ss.str();
}

void GameAPI::RegisterBuiltinAPI() {
    RegisterEntityAPI();
    RegisterCombatAPI();
    RegisterQueryAPI();
    RegisterAudioVisualAPI();
    RegisterUIAPI();
    RegisterUtilityAPI();
}

void GameAPI::RegisterEntityAPI() {
    RegisterFunction({
        "spawn_entity", "nova.spawn_entity",
        "spawn_entity(type: str, x: float, y: float, z: float) -> int",
        "Spawn a new entity at the specified position",
        "Creates a new entity of the given type at world coordinates (x, y, z). Returns the entity ID which can be used to reference the entity in other API calls.",
        "Entity",
        {{"type", "str", "Entity type identifier", "", false},
         {"x", "float", "X position", "", false},
         {"y", "float", "Y position", "", false},
         {"z", "float", "Z position", "", false}},
        "int", "The ID of the spawned entity",
        "enemy_id = spawn_entity('zombie', 10.0, 0.0, 15.0)",
        {"despawn_entity", "get_position"}, {"entity", "spawn"}, false, "", "1.0"
    });

    RegisterFunction({
        "despawn_entity", "nova.despawn_entity",
        "despawn_entity(entity_id: int) -> None",
        "Remove an entity from the world",
        "Removes the entity with the given ID from the game world. The entity will be destroyed and its ID will no longer be valid.",
        "Entity",
        {{"entity_id", "int", "ID of the entity to remove", "", false}},
        "None", "",
        "despawn_entity(enemy_id)",
        {"spawn_entity"}, {"entity"}, false, "", "1.0"
    });

    RegisterFunction({
        "get_position", "nova.get_position",
        "get_position(entity_id: int) -> Vec3",
        "Get the world position of an entity",
        "Returns a Vec3 containing the entity's current position in world coordinates.",
        "Entity",
        {{"entity_id", "int", "ID of the entity", "", false}},
        "Vec3", "The entity's position",
        "pos = get_position(player_id)\nprint(f'At {pos.x}, {pos.y}, {pos.z}')",
        {"set_position"}, {"entity", "position"}, false, "", "1.0"
    });

    RegisterFunction({
        "set_position", "nova.set_position",
        "set_position(entity_id: int, x: float, y: float, z: float) -> None",
        "Set the world position of an entity",
        "Teleports the entity to the specified world coordinates.",
        "Entity",
        {{"entity_id", "int", "ID of the entity", "", false},
         {"x", "float", "X position", "", false},
         {"y", "float", "Y position", "", false},
         {"z", "float", "Z position", "", false}},
        "None", "",
        "set_position(player_id, 0.0, 0.0, 0.0)",
        {"get_position"}, {"entity", "position"}, false, "", "1.0"
    });
}

void GameAPI::RegisterCombatAPI() {
    RegisterFunction({
        "damage", "nova.damage",
        "damage(target_id: int, amount: float, source_id: int = 0) -> None",
        "Apply damage to an entity",
        "Reduces the target entity's health by the specified amount. Optionally specify the source entity for attribution.",
        "Combat",
        {{"target_id", "int", "ID of the entity to damage", "", false},
         {"amount", "float", "Amount of damage", "", false},
         {"source_id", "int", "ID of the damage source", "0", true}},
        "None", "",
        "damage(enemy_id, 50.0, player_id)",
        {"heal", "get_health"}, {"combat", "damage"}, false, "", "1.0"
    });

    RegisterFunction({
        "heal", "nova.heal",
        "heal(target_id: int, amount: float) -> None",
        "Heal an entity",
        "Restores health to the target entity, up to its maximum health.",
        "Combat",
        {{"target_id", "int", "ID of the entity to heal", "", false},
         {"amount", "float", "Amount to heal", "", false}},
        "None", "",
        "heal(ally_id, 25.0)",
        {"damage", "get_health"}, {"combat", "heal"}, false, "", "1.0"
    });

    RegisterFunction({
        "get_health", "nova.get_health",
        "get_health(entity_id: int) -> float",
        "Get the current health of an entity",
        "Returns the entity's current health value.",
        "Combat",
        {{"entity_id", "int", "ID of the entity", "", false}},
        "float", "Current health",
        "if get_health(player_id) < 20:\n    show_notification('Low health!')",
        {"damage", "heal", "is_alive"}, {"combat", "health"}, false, "", "1.0"
    });

    RegisterFunction({
        "is_alive", "nova.is_alive",
        "is_alive(entity_id: int) -> bool",
        "Check if an entity is alive",
        "Returns True if the entity exists and has health > 0.",
        "Combat",
        {{"entity_id", "int", "ID of the entity", "", false}},
        "bool", "True if alive",
        "if is_alive(target_id):\n    damage(target_id, 10.0)",
        {"get_health"}, {"combat"}, false, "", "1.0"
    });
}

void GameAPI::RegisterQueryAPI() {
    RegisterFunction({
        "find_entities_in_radius", "nova.find_entities_in_radius",
        "find_entities_in_radius(x: float, y: float, z: float, radius: float) -> List[int]",
        "Find all entities within a radius",
        "Returns a list of entity IDs for all entities within the specified radius of the point.",
        "Query",
        {{"x", "float", "Center X coordinate", "", false},
         {"y", "float", "Center Y coordinate", "", false},
         {"z", "float", "Center Z coordinate", "", false},
         {"radius", "float", "Search radius", "", false}},
        "List[int]", "List of entity IDs",
        "nearby = find_entities_in_radius(0, 0, 0, 10.0)\nfor eid in nearby:\n    print(eid)",
        {"get_distance"}, {"query", "spatial"}, false, "", "1.0"
    });

    RegisterFunction({
        "get_distance", "nova.get_distance",
        "get_distance(entity1: int, entity2: int) -> float",
        "Get distance between two entities",
        "Returns the Euclidean distance between two entities.",
        "Query",
        {{"entity1", "int", "First entity ID", "", false},
         {"entity2", "int", "Second entity ID", "", false}},
        "float", "Distance in world units",
        "if get_distance(player, enemy) < 5.0:\n    attack(enemy)",
        {"find_entities_in_radius"}, {"query", "distance"}, false, "", "1.0"
    });
}

void GameAPI::RegisterAudioVisualAPI() {
    RegisterFunction({
        "play_sound", "nova.play_sound",
        "play_sound(name: str, x: float = 0, y: float = 0, z: float = 0) -> None",
        "Play a sound effect",
        "Plays the named sound at the specified position. If no position given, plays at listener.",
        "Audio",
        {{"name", "str", "Sound effect name", "", false},
         {"x", "float", "X position", "0", true},
         {"y", "float", "Y position", "0", true},
         {"z", "float", "Z position", "0", true}},
        "None", "",
        "play_sound('explosion', pos.x, pos.y, pos.z)",
        {"spawn_effect"}, {"audio", "sound"}, false, "", "1.0"
    });

    RegisterFunction({
        "spawn_effect", "nova.spawn_effect",
        "spawn_effect(name: str, x: float, y: float, z: float) -> None",
        "Spawn a visual effect",
        "Spawns the named particle/visual effect at the position.",
        "Visual",
        {{"name", "str", "Effect name", "", false},
         {"x", "float", "X position", "", false},
         {"y", "float", "Y position", "", false},
         {"z", "float", "Z position", "", false}},
        "None", "",
        "spawn_effect('fire_explosion', 10.0, 0.0, 15.0)",
        {"play_sound"}, {"visual", "effect"}, false, "", "1.0"
    });
}

void GameAPI::RegisterUIAPI() {
    RegisterFunction({
        "show_notification", "nova.show_notification",
        "show_notification(message: str, duration: float = 3.0) -> None",
        "Show a UI notification",
        "Displays a notification message to the player.",
        "UI",
        {{"message", "str", "Message to display", "", false},
         {"duration", "float", "Duration in seconds", "3.0", true}},
        "None", "",
        "show_notification('Quest completed!', 5.0)",
        {}, {"ui", "notification"}, false, "", "1.0"
    });
}

void GameAPI::RegisterUtilityAPI() {
    RegisterFunction({
        "get_delta_time", "nova.get_delta_time",
        "get_delta_time() -> float",
        "Get frame delta time",
        "Returns the time elapsed since the last frame in seconds.",
        "Time",
        {},
        "float", "Delta time in seconds",
        "timer -= get_delta_time()",
        {"get_game_time"}, {"time"}, false, "", "1.0"
    });

    RegisterFunction({
        "get_game_time", "nova.get_game_time",
        "get_game_time() -> float",
        "Get total game time",
        "Returns the total elapsed game time in seconds.",
        "Time",
        {},
        "float", "Total game time",
        "if get_game_time() > 300:\n    spawn_boss()",
        {"get_delta_time"}, {"time"}, false, "", "1.0"
    });

    RegisterFunction({
        "random", "nova.random",
        "random() -> float",
        "Get random float 0-1",
        "Returns a random float between 0.0 and 1.0.",
        "Math",
        {},
        "float", "Random value",
        "if random() < 0.1:\n    drop_loot()",
        {"random_range"}, {"math", "random"}, false, "", "1.0"
    });

    RegisterFunction({
        "random_range", "nova.random_range",
        "random_range(min: float, max: float) -> float",
        "Get random float in range",
        "Returns a random float between min and max.",
        "Math",
        {{"min", "float", "Minimum value", "", false},
         {"max", "float", "Maximum value", "", false}},
        "float", "Random value in range",
        "damage_amount = random_range(10.0, 20.0)",
        {"random"}, {"math", "random"}, false, "", "1.0"
    });

    RegisterFunction({
        "log", "nova.log",
        "log(message: str) -> None",
        "Log message to console",
        "Writes an info message to the game console.",
        "Debug",
        {{"message", "str", "Message to log", "", false}},
        "None", "",
        "log(f'Player health: {get_health(player_id)}')",
        {}, {"debug", "log"}, false, "", "1.0"
    });
}

void GameAPI::RegisterBuiltinTypes() {
    APITypeDoc vec3;
    vec3.name = "Vec3";
    vec3.description = "3D vector with x, y, z components";
    vec3.category = "Math";
    vec3.properties = {
        {"x", "float", "X component", "", false},
        {"y", "float", "Y component", "", false},
        {"z", "float", "Z component", "", false}
    };
    RegisterType(vec3);

    APITypeDoc entity;
    entity.name = "Entity";
    entity.description = "Game entity reference";
    entity.category = "Core";
    entity.properties = {
        {"id", "int", "Entity ID", "", false},
        {"type", "str", "Entity type", "", false},
        {"position", "Vec3", "World position", "", false}
    };
    RegisterType(entity);
}

void GameAPI::RegisterBuiltinEvents() {
    RegisterEvent({
        "OnCreate", "Called when entity is spawned", "Entity",
        {{"entity_id", "int", "Created entity ID", "", false}},
        "def on_create(entity_id: int) -> None:\n    log(f'Entity {entity_id} created')",
        "on_create(entity_id: int) -> None"
    });

    RegisterEvent({
        "OnTick", "Called every frame", "Entity",
        {{"entity_id", "int", "Entity ID", "", false}},
        "def on_tick(entity_id: int) -> None:\n    dt = get_delta_time()",
        "on_tick(entity_id: int) -> None"
    });

    RegisterEvent({
        "OnDamage", "Called when entity takes damage", "Combat",
        {{"entity_id", "int", "Damaged entity ID", "", false},
         {"damage", "float", "Damage amount", "", false},
         {"source_id", "int", "Damage source ID", "", false}},
        "def on_damage(entity_id: int, damage: float, source_id: int) -> None:\n    pass",
        "on_damage(entity_id: int, damage: float, source_id: int) -> None"
    });

    RegisterEvent({
        "OnDeath", "Called when entity dies", "Combat",
        {{"entity_id", "int", "Dead entity ID", "", false},
         {"killer_id", "int", "Killer entity ID", "", false}},
        "def on_death(entity_id: int, killer_id: int) -> None:\n    pass",
        "on_death(entity_id: int, killer_id: int) -> None"
    });
}

} // namespace Scripting
} // namespace Nova
