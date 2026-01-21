#include "ScriptTemplate.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

namespace Nova {
namespace Scripting {

ScriptTemplateManager::ScriptTemplateManager() = default;
ScriptTemplateManager::~ScriptTemplateManager() {
    Shutdown();
}

bool ScriptTemplateManager::Initialize(const std::string& templatesPath) {
    if (m_initialized) return true;

    m_templatesPath = templatesPath;

    // Register built-in templates
    RegisterBuiltinTemplates();

    // Load custom templates if path provided
    if (!templatesPath.empty() && fs::exists(templatesPath)) {
        LoadTemplates(templatesPath);
    }

    m_initialized = true;
    return true;
}

void ScriptTemplateManager::Shutdown() {
    m_templates.clear();
    m_templateIndex.clear();
    m_initialized = false;
}

std::optional<ScriptTemplate> ScriptTemplateManager::GetTemplate(const std::string& id) const {
    auto it = m_templateIndex.find(id);
    if (it != m_templateIndex.end()) {
        return m_templates[it->second];
    }
    return std::nullopt;
}

std::vector<ScriptTemplate> ScriptTemplateManager::GetTemplatesByCategory(const std::string& category) const {
    std::vector<ScriptTemplate> result;
    for (const auto& tmpl : m_templates) {
        if (tmpl.category == category) {
            result.push_back(tmpl);
        }
    }
    return result;
}

std::vector<std::string> ScriptTemplateManager::GetCategories() const {
    std::vector<std::string> categories;
    for (const auto& tmpl : m_templates) {
        if (std::find(categories.begin(), categories.end(), tmpl.category) == categories.end()) {
            categories.push_back(tmpl.category);
        }
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

std::vector<ScriptTemplate> ScriptTemplateManager::SearchTemplates(const std::string& query) const {
    std::vector<ScriptTemplate> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& tmpl : m_templates) {
        std::string lowerName = tmpl.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string lowerDesc = tmpl.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDesc.find(lowerQuery) != std::string::npos) {
            result.push_back(tmpl);
        }
    }
    return result;
}

std::string ScriptTemplateManager::Generate(const std::string& templateId,
                                            const std::unordered_map<std::string, std::string>& variables) {
    auto tmpl = GetTemplate(templateId);
    if (!tmpl) return "";

    // Fill in default values for missing variables
    std::unordered_map<std::string, std::string> allVars = variables;
    for (const auto& var : tmpl->variables) {
        if (allVars.find(var.name) == allVars.end()) {
            allVars[var.name] = var.defaultValue;
        }
    }

    return SubstituteVariables(tmpl->content, allVars);
}

std::string ScriptTemplateManager::GenerateFromContent(const std::string& content,
                                                       const std::unordered_map<std::string, std::string>& variables) {
    return SubstituteVariables(content, variables);
}

bool ScriptTemplateManager::ValidateVariables(const std::string& templateId,
                                              const std::unordered_map<std::string, std::string>& variables,
                                              std::vector<std::string>& errors) {
    auto tmpl = GetTemplate(templateId);
    if (!tmpl) {
        errors.push_back("Template not found: " + templateId);
        return false;
    }

    bool valid = true;

    for (const auto& var : tmpl->variables) {
        auto it = variables.find(var.name);

        if (var.required && (it == variables.end() || it->second.empty())) {
            errors.push_back("Required variable missing: " + var.name);
            valid = false;
            continue;
        }

        if (it != variables.end() && !it->second.empty()) {
            // Type validation
            if (var.type == "int") {
                try {
                    std::stoi(it->second);
                } catch (...) {
                    errors.push_back("Variable '" + var.name + "' must be an integer");
                    valid = false;
                }
            } else if (var.type == "float") {
                try {
                    std::stof(it->second);
                } catch (...) {
                    errors.push_back("Variable '" + var.name + "' must be a number");
                    valid = false;
                }
            } else if (var.type == "select") {
                if (std::find(var.options.begin(), var.options.end(), it->second) == var.options.end()) {
                    errors.push_back("Variable '" + var.name + "' must be one of the allowed options");
                    valid = false;
                }
            }
        }
    }

    return valid;
}

void ScriptTemplateManager::RegisterTemplate(const ScriptTemplate& tmpl) {
    auto it = m_templateIndex.find(tmpl.id);
    if (it != m_templateIndex.end()) {
        m_templates[it->second] = tmpl;
    } else {
        m_templates.push_back(tmpl);
        m_templateIndex[tmpl.id] = m_templates.size() - 1;
    }
}

void ScriptTemplateManager::RemoveTemplate(const std::string& id) {
    auto it = m_templateIndex.find(id);
    if (it != m_templateIndex.end()) {
        m_templates.erase(m_templates.begin() + it->second);
        m_templateIndex.erase(it);

        // Rebuild index
        m_templateIndex.clear();
        for (size_t i = 0; i < m_templates.size(); ++i) {
            m_templateIndex[m_templates[i].id] = i;
        }
    }
}

int ScriptTemplateManager::LoadTemplates(const std::string& directory) {
    int loaded = 0;

    if (!fs::exists(directory)) return 0;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".template" || ext == ".py") {
                auto tmpl = ParseTemplateFile(entry.path().string());
                if (!tmpl.id.empty()) {
                    RegisterTemplate(tmpl);
                    loaded++;
                }
            }
        }
    }

    return loaded;
}

bool ScriptTemplateManager::SaveTemplate(const ScriptTemplate& tmpl, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    // Write header with metadata
    file << "# Template: " << tmpl.name << "\n";
    file << "# Category: " << tmpl.category << "\n";
    file << "# Description: " << tmpl.description << "\n";
    file << "# Variables:\n";
    for (const auto& var : tmpl.variables) {
        file << "#   " << var.name << " (" << var.type << "): " << var.description << "\n";
    }
    file << "\n";
    file << tmpl.content;

    return file.good();
}

ScriptTemplate ScriptTemplateManager::GetOnCreateTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "on_create";
    tmpl.name = "OnCreate Handler";
    tmpl.category = "Events";
    tmpl.description = "Handler called when an entity is created/spawned";
    tmpl.variables = {
        {"entity_type", "entity", "Type of entity this handles", "string", {}, false},
        {"author", "Unknown", "Script author", "string", {}, false}
    };
    tmpl.content = R"PYTEMPLATE("""
OnCreate handler for {{entity_type}}
@author: {{author}}
"""

def on_create(entity_id: int) -> None:
    """Called when the entity is created."""
    # Get initial position
    pos = get_position(entity_id)

    # Initialize entity state
    log(f"Entity {entity_id} created at ({pos.x}, {pos.y}, {pos.z})")

    # Add your initialization logic here
    pass
)PYTEMPLATE";
    tmpl.cursorLine = 13;
    tmpl.cursorColumn = 4;
    return tmpl;
}

ScriptTemplate ScriptTemplateManager::GetOnTickTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "on_tick";
    tmpl.name = "OnTick Handler";
    tmpl.category = "Events";
    tmpl.description = "Handler called every frame for an entity";
    tmpl.variables = {
        {"entity_type", "entity", "Type of entity", "string", {}, false},
        {"tick_rate", "1.0", "How often to execute (seconds)", "float", {}, false}
    };
    tmpl.content = R"("""
OnTick handler for {{entity_type}}
Tick rate: {{tick_rate}}s
"""

# Timer for rate limiting
_tick_timer = 0.0

def on_tick(entity_id: int) -> None:
    """Called every frame. Use timer for rate limiting."""
    global _tick_timer

    dt = get_delta_time()
    _tick_timer += dt

    # Rate limit to {{tick_rate}} seconds
    if _tick_timer < {{tick_rate}}:
        return
    _tick_timer = 0.0

    # Entity is alive check
    if not is_alive(entity_id):
        return

    # Get entity state
    pos = get_position(entity_id)
    health = get_health(entity_id)

    # Add your per-tick logic here
    pass
)";
    tmpl.cursorLine = 27;
    tmpl.cursorColumn = 4;
    return tmpl;
}

ScriptTemplate ScriptTemplateManager::GetOnEventTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "on_event";
    tmpl.name = "Generic Event Handler";
    tmpl.category = "Events";
    tmpl.description = "Handler for custom game events";
    tmpl.variables = {
        {"event_name", "custom_event", "Name of the event", "string", {}, true},
        {"event_params", "data", "Event parameters", "string", {}, false}
    };
    tmpl.content = R"("""
Handler for {{event_name}} event
"""

def on_{{event_name}}({{event_params}}) -> None:
    """Handle the {{event_name}} event."""
    log(f"{{event_name}} triggered")

    # Process event data
    # Add your event handling logic here
    pass
)";
    return tmpl;
}

ScriptTemplate ScriptTemplateManager::GetAIBehaviorTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "ai_behavior";
    tmpl.name = "AI Behavior Script";
    tmpl.category = "AI";
    tmpl.description = "AI behavior with state machine pattern";
    tmpl.variables = {
        {"ai_name", "custom_ai", "Name of the AI behavior", "string", {}, true},
        {"states", "idle,patrol,chase,attack", "Comma-separated state names", "string", {}, false},
        {"detection_range", "10.0", "Range to detect targets", "float", {}, false},
        {"attack_range", "2.0", "Range to attack targets", "float", {}, false}
    };
    tmpl.content = R"("""
AI Behavior: {{ai_name}}
States: {{states}}

@author: Game Designer
@version: 1.0
"""

from enum import Enum
from typing import Optional

class AIState(Enum):
    IDLE = "idle"
    PATROL = "patrol"
    CHASE = "chase"
    ATTACK = "attack"

class {{ai_name}}:
    """AI controller for this entity type."""

    def __init__(self, entity_id: int):
        self.entity_id = entity_id
        self.state = AIState.IDLE
        self.target_id: Optional[int] = None
        self.patrol_points: list = []
        self.current_patrol_index = 0
        self.detection_range = {{detection_range}}
        self.attack_range = {{attack_range}}

    def update(self, dt: float) -> None:
        """Called each frame to update AI."""
        if not is_alive(self.entity_id):
            return

        # State machine
        if self.state == AIState.IDLE:
            self._update_idle(dt)
        elif self.state == AIState.PATROL:
            self._update_patrol(dt)
        elif self.state == AIState.CHASE:
            self._update_chase(dt)
        elif self.state == AIState.ATTACK:
            self._update_attack(dt)

    def _update_idle(self, dt: float) -> None:
        """Idle state - look for targets."""
        target = self._find_target()
        if target:
            self.target_id = target
            self._change_state(AIState.CHASE)
        elif self.patrol_points:
            self._change_state(AIState.PATROL)

    def _update_patrol(self, dt: float) -> None:
        """Patrol state - move between patrol points."""
        # Check for targets
        target = self._find_target()
        if target:
            self.target_id = target
            self._change_state(AIState.CHASE)
            return

        # Move to current patrol point
        if self.patrol_points:
            point = self.patrol_points[self.current_patrol_index]
            pos = get_position(self.entity_id)
            dist = ((pos.x - point[0])**2 + (pos.z - point[2])**2)**0.5

            if dist < 1.0:
                self.current_patrol_index = (self.current_patrol_index + 1) % len(self.patrol_points)
            else:
                move_to(self.entity_id, point[0], point[1], point[2])

    def _update_chase(self, dt: float) -> None:
        """Chase state - pursue target."""
        if not self.target_id or not is_alive(self.target_id):
            self.target_id = None
            self._change_state(AIState.IDLE)
            return

        target_pos = get_position(self.target_id)
        my_pos = get_position(self.entity_id)
        dist = get_distance(self.entity_id, self.target_id)

        if dist > self.detection_range * 1.5:
            # Lost target
            self.target_id = None
            self._change_state(AIState.IDLE)
        elif dist <= self.attack_range:
            self._change_state(AIState.ATTACK)
        else:
            move_to(self.entity_id, target_pos.x, target_pos.y, target_pos.z)

    def _update_attack(self, dt: float) -> None:
        """Attack state - attack target."""
        if not self.target_id or not is_alive(self.target_id):
            self.target_id = None
            self._change_state(AIState.IDLE)
            return

        dist = get_distance(self.entity_id, self.target_id)
        if dist > self.attack_range:
            self._change_state(AIState.CHASE)
        else:
            # Perform attack
            damage(self.target_id, 10.0, self.entity_id)

    def _find_target(self) -> Optional[int]:
        """Find nearest enemy in detection range."""
        pos = get_position(self.entity_id)
        nearby = find_entities_in_radius(pos.x, pos.y, pos.z, self.detection_range)

        for eid in nearby:
            if eid != self.entity_id and is_alive(eid):
                # Add team/faction check here
                return eid
        return None

    def _change_state(self, new_state: AIState) -> None:
        """Change to a new AI state."""
        log(f"AI {self.entity_id}: {self.state.value} -> {new_state.value}")
        self.state = new_state


# Global AI instance (created per entity)
_ai_instance: Optional[{{ai_name}}] = None

def on_create(entity_id: int) -> None:
    global _ai_instance
    _ai_instance = {{ai_name}}(entity_id)

def on_tick(entity_id: int) -> None:
    if _ai_instance:
        _ai_instance.update(get_delta_time())
)";
    return tmpl;
}

ScriptTemplate ScriptTemplateManager::GetSpellEffectTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "spell_effect";
    tmpl.name = "Spell Effect Script";
    tmpl.category = "Combat";
    tmpl.description = "Script for spell/ability effects";
    tmpl.variables = {
        {"spell_name", "fireball", "Name of the spell", "string", {}, true},
        {"base_damage", "50", "Base damage amount", "float", {}, false},
        {"aoe_radius", "0", "Area of effect radius (0 for single target)", "float", {}, false},
        {"effect_name", "fire_explosion", "Visual effect to spawn", "string", {}, false},
        {"sound_name", "spell_fire", "Sound effect to play", "string", {}, false}
    };
    tmpl.content = R"("""
Spell Effect: {{spell_name}}
Base Damage: {{base_damage}}
AoE Radius: {{aoe_radius}}
"""

def calculate_damage(caster_id: int, target_id: int) -> float:
    """Calculate final damage based on caster stats."""
    base = {{base_damage}}

    # Could add caster stat bonuses here
    # spell_power = get_stat(caster_id, "spell_power")
    # base *= (1 + spell_power / 100)

    return base

def on_spell_hit(caster_id: int, target_id: int, hit_pos: tuple) -> None:
    """Called when spell hits target/location."""
    x, y, z = hit_pos

    # Spawn visual effect
    spawn_effect("{{effect_name}}", x, y, z)
    play_sound("{{sound_name}}", x, y, z)

    aoe_radius = {{aoe_radius}}

    if aoe_radius > 0:
        # Area of effect damage
        targets = find_entities_in_radius(x, y, z, aoe_radius)
        for tid in targets:
            if is_alive(tid) and tid != caster_id:
                dmg = calculate_damage(caster_id, tid)
                # Damage falloff from center
                dist = get_distance(target_id, tid) if target_id else 0
                falloff = 1.0 - (dist / aoe_radius) * 0.5
                damage(tid, dmg * falloff, caster_id)
    else:
        # Single target damage
        if target_id and is_alive(target_id):
            dmg = calculate_damage(caster_id, target_id)
            damage(target_id, dmg, caster_id)

def on_spell_cast(caster_id: int) -> bool:
    """Called when spell is cast. Return False to cancel."""
    # Could add mana cost check, cooldown check, etc.
    log(f"{{spell_name}} cast by {caster_id}")
    return True
)";
    return tmpl;
}

ScriptTemplate ScriptTemplateManager::GetConditionTemplate() const {
    ScriptTemplate tmpl;
    tmpl.id = "condition_check";
    tmpl.name = "Condition Check Script";
    tmpl.category = "Utility";
    tmpl.description = "Reusable condition checking function";
    tmpl.variables = {
        {"condition_name", "is_valid_target", "Name of the condition", "string", {}, true},
        {"description", "Check if target is valid", "What this condition checks", "string", {}, false}
    };
    tmpl.content = R"("""
Condition: {{condition_name}}
{{description}}
"""

def {{condition_name}}(entity_id: int, **kwargs) -> bool:
    """
    {{description}}

    Args:
        entity_id: The entity to check
        **kwargs: Additional parameters

    Returns:
        bool: True if condition is met
    """
    # Basic validation
    if not is_alive(entity_id):
        return False

    # Add your condition logic here
    # Example checks:
    # - Health threshold: get_health(entity_id) > threshold
    # - Distance check: get_distance(entity_id, target) < range
    # - State check: get_state(entity_id) == expected_state

    return True


# Convenience wrappers for common conditions
def is_low_health(entity_id: int, threshold: float = 0.25) -> bool:
    """Check if entity health is below threshold (0-1)."""
    health = get_health(entity_id)
    max_health = 100  # Would get from entity stats
    return (health / max_health) < threshold

def is_in_range(entity_id: int, target_id: int, range: float) -> bool:
    """Check if entity is within range of target."""
    return get_distance(entity_id, target_id) <= range

def has_clear_line_of_sight(entity_id: int, target_id: int) -> bool:
    """Check if entity can see target (no obstacles)."""
    # Would use raycast in full implementation
    return True
)";
    return tmpl;
}

void ScriptTemplateManager::RegisterBuiltinTemplates() {
    RegisterTemplate(GetOnCreateTemplate());
    RegisterTemplate(GetOnTickTemplate());
    RegisterTemplate(GetOnEventTemplate());
    RegisterTemplate(GetAIBehaviorTemplate());
    RegisterTemplate(GetSpellEffectTemplate());
    RegisterTemplate(GetConditionTemplate());

    // On Damage template
    ScriptTemplate onDamage;
    onDamage.id = "on_damage";
    onDamage.name = "OnDamage Handler";
    onDamage.category = "Events";
    onDamage.description = "Handler called when entity takes damage";
    onDamage.content = R"("""
OnDamage handler
"""

def on_damage(entity_id: int, damage: float, source_id: int) -> None:
    """Called when entity takes damage."""
    log(f"Entity {entity_id} took {damage} damage from {source_id}")

    # Check for death
    health = get_health(entity_id)
    if health <= 0:
        on_death(entity_id, source_id)
        return

    # Low health warning
    if health < 20:
        show_notification("Low health!", 1.0)

    # Could trigger effects here
    # spawn_effect("damage_flash", *get_position(entity_id))

def on_death(entity_id: int, killer_id: int) -> None:
    """Called when entity dies."""
    log(f"Entity {entity_id} killed by {killer_id}")

    # Drop loot, grant XP, etc.
    pos = get_position(entity_id)
    spawn_effect("death_effect", pos.x, pos.y, pos.z)
)";
    RegisterTemplate(onDamage);

    // Tech Unlock template
    ScriptTemplate techUnlock;
    techUnlock.id = "tech_unlock";
    techUnlock.name = "Tech Unlock Handler";
    techUnlock.category = "RTS";
    techUnlock.description = "Handler for technology/research completion";
    techUnlock.variables = {
        {"tech_id", "tech_name", "Technology identifier", "string", {}, true}
    };
    techUnlock.content = R"("""
Tech Unlock Handler: {{tech_id}}
"""

def on_tech_unlocked(player_id: int, tech_id: str) -> None:
    """Called when a technology is researched."""
    if tech_id != "{{tech_id}}":
        return

    log(f"Player {player_id} unlocked {tech_id}")
    show_notification(f"Technology unlocked: {tech_id}", 3.0)

    # Apply tech bonuses
    # apply_tech_bonus(player_id, tech_id)

    # Unlock new buildings/units
    # unlock_building(player_id, "advanced_barracks")

    # Play celebration effect
    play_sound("tech_complete")
)";
    RegisterTemplate(techUnlock);
}

std::string ScriptTemplateManager::SubstituteVariables(const std::string& content,
                                                       const std::unordered_map<std::string, std::string>& variables) {
    std::string result = content;

    for (const auto& [name, value] : variables) {
        std::string placeholder = "{{" + name + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return result;
}

ScriptTemplate ScriptTemplateManager::ParseTemplateFile(const std::string& path) {
    ScriptTemplate tmpl;

    std::ifstream file(path);
    if (!file.is_open()) return tmpl;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Extract metadata from comments
    std::regex nameRegex(R"(#\s*Template:\s*(.+))");
    std::regex catRegex(R"(#\s*Category:\s*(.+))");
    std::regex descRegex(R"(#\s*Description:\s*(.+))");

    std::smatch match;
    if (std::regex_search(content, match, nameRegex)) {
        tmpl.name = match[1].str();
    }
    if (std::regex_search(content, match, catRegex)) {
        tmpl.category = match[1].str();
    }
    if (std::regex_search(content, match, descRegex)) {
        tmpl.description = match[1].str();
    }

    // Use filename as ID
    tmpl.id = fs::path(path).stem().string();
    tmpl.content = content;

    // Extract variable definitions
    std::regex varRegex(R"(#\s*(\w+)\s*\((\w+)\):\s*(.+))");
    std::string::const_iterator searchStart(content.cbegin());
    while (std::regex_search(searchStart, content.cend(), match, varRegex)) {
        TemplateVariable var;
        var.name = match[1].str();
        var.type = match[2].str();
        var.description = match[3].str();
        tmpl.variables.push_back(var);
        searchStart = match.suffix().first;
    }

    return tmpl;
}

} // namespace Scripting
} // namespace Nova
