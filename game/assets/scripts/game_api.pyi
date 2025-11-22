"""
Game API Type Stubs
Auto-generated Python type stubs for the Nova Game Engine scripting API.

This file provides type hints for IDE auto-completion and type checking.
Do not modify directly - regenerate using GameAPI::GenerateTypeStubs().

Version: 1.0.0
Generated: 2024
"""

from typing import List, Tuple, Dict, Any, Optional, Callable, Union, overload


# =============================================================================
# Entity System
# =============================================================================

def spawn_entity(entity_type: str, x: float, y: float, z: float) -> int:
    """
    Spawn a new entity at the specified position.

    Args:
        entity_type: Entity type identifier (e.g., "zombie", "soldier")
        x: X coordinate in world space
        y: Y coordinate in world space
        z: Z coordinate in world space

    Returns:
        The unique ID of the spawned entity, or 0 on failure

    Example:
        enemy_id = spawn_entity("zombie", 10.0, 0.0, 15.0)
    """
    ...


def destroy_entity(entity_id: int) -> bool:
    """
    Destroy an entity immediately.

    Args:
        entity_id: The ID of the entity to destroy

    Returns:
        True if entity was destroyed, False if not found
    """
    ...


def get_position(entity_id: int) -> Tuple[float, float, float]:
    """
    Get the world position of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Tuple of (x, y, z) coordinates

    Example:
        x, y, z = get_position(entity_id)
    """
    ...


def set_position(entity_id: int, x: float, y: float, z: float) -> None:
    """
    Set the world position of an entity.

    Args:
        entity_id: The entity ID
        x: X coordinate
        y: Y coordinate
        z: Z coordinate
    """
    ...


def get_rotation(entity_id: int) -> Tuple[float, float, float]:
    """
    Get the rotation of an entity in euler angles (degrees).

    Args:
        entity_id: The entity ID

    Returns:
        Tuple of (pitch, yaw, roll) in degrees
    """
    ...


def set_rotation(entity_id: int, pitch: float, yaw: float, roll: float) -> None:
    """
    Set the rotation of an entity in euler angles (degrees).

    Args:
        entity_id: The entity ID
        pitch: Rotation around X axis (degrees)
        yaw: Rotation around Y axis (degrees)
        roll: Rotation around Z axis (degrees)
    """
    ...


def get_velocity(entity_id: int) -> Tuple[float, float, float]:
    """
    Get the velocity of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Tuple of (vx, vy, vz) velocity components
    """
    ...


def set_velocity(entity_id: int, vx: float, vy: float, vz: float) -> None:
    """
    Set the velocity of an entity.

    Args:
        entity_id: The entity ID
        vx: X velocity component
        vy: Y velocity component
        vz: Z velocity component
    """
    ...


def get_entity_type(entity_id: int) -> str:
    """
    Get the type identifier of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        The entity type string (e.g., "zombie", "soldier")
    """
    ...


def is_alive(entity_id: int) -> bool:
    """
    Check if an entity is alive (health > 0).

    Args:
        entity_id: The entity ID

    Returns:
        True if entity exists and is alive
    """
    ...


def get_owner(entity_id: int) -> int:
    """
    Get the owner/player ID of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        The owner player ID, or 0 if no owner
    """
    ...


def set_owner(entity_id: int, player_id: int) -> None:
    """
    Set the owner/player ID of an entity.

    Args:
        entity_id: The entity ID
        player_id: The new owner player ID
    """
    ...


def look_at(entity_id: int, x: float, y: float, z: float) -> None:
    """
    Rotate an entity to face a position.

    Args:
        entity_id: The entity ID
        x: Target X coordinate
        y: Target Y coordinate
        z: Target Z coordinate
    """
    ...


def move_towards(entity_id: int, x: float, y: float, z: float, speed: float) -> None:
    """
    Move an entity towards a target position.

    Args:
        entity_id: The entity ID
        x: Target X coordinate
        y: Target Y coordinate
        z: Target Z coordinate
        speed: Movement speed (units per second)
    """
    ...


def command_move(entity_id: int, x: float, y: float, z: float) -> None:
    """
    Issue a move command to a unit (pathfinding).

    Args:
        entity_id: The entity ID
        x: Destination X coordinate
        y: Destination Y coordinate
        z: Destination Z coordinate
    """
    ...


# =============================================================================
# Combat System
# =============================================================================

def get_health(entity_id: int) -> float:
    """
    Get the current health of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Current health value
    """
    ...


def set_health(entity_id: int, health: float) -> None:
    """
    Set the current health of an entity.

    Args:
        entity_id: The entity ID
        health: New health value
    """
    ...


def get_max_health(entity_id: int) -> float:
    """
    Get the maximum health of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Maximum health value
    """
    ...


def set_max_health(entity_id: int, max_health: float) -> None:
    """
    Set the maximum health of an entity.

    Args:
        entity_id: The entity ID
        max_health: New maximum health value
    """
    ...


def get_mana(entity_id: int) -> float:
    """
    Get the current mana of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Current mana value
    """
    ...


def set_mana(entity_id: int, mana: float) -> None:
    """
    Set the current mana of an entity.

    Args:
        entity_id: The entity ID
        mana: New mana value
    """
    ...


def get_max_mana(entity_id: int) -> float:
    """
    Get the maximum mana of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Maximum mana value
    """
    ...


def set_max_mana(entity_id: int, max_mana: float) -> None:
    """
    Set the maximum mana of an entity.

    Args:
        entity_id: The entity ID
        max_mana: New maximum mana value
    """
    ...


def get_armor(entity_id: int) -> float:
    """
    Get the armor value of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Armor value
    """
    ...


def set_armor(entity_id: int, armor: float) -> None:
    """
    Set the armor value of an entity.

    Args:
        entity_id: The entity ID
        armor: New armor value
    """
    ...


def damage(target_id: int, amount: float, damage_type: str, source_id: int = 0) -> float:
    """
    Apply damage to an entity.

    Args:
        target_id: The entity to damage
        amount: Damage amount
        damage_type: Type of damage ("physical", "magical", "true")
        source_id: The entity dealing damage (optional)

    Returns:
        Actual damage dealt after resistances
    """
    ...


def heal(target_id: int, amount: float, source_id: int = 0) -> float:
    """
    Heal an entity.

    Args:
        target_id: The entity to heal
        amount: Heal amount
        source_id: The entity providing healing (optional)

    Returns:
        Actual amount healed
    """
    ...


def is_enemy(entity_id: int, other_id: int) -> bool:
    """
    Check if two entities are enemies.

    Args:
        entity_id: First entity ID
        other_id: Second entity ID

    Returns:
        True if they are enemies
    """
    ...


def is_ally(entity_id: int, other_id: int) -> bool:
    """
    Check if two entities are allies.

    Args:
        entity_id: First entity ID
        other_id: Second entity ID

    Returns:
        True if they are allies
    """
    ...


def get_target(entity_id: int) -> int:
    """
    Get the current target of an entity.

    Args:
        entity_id: The entity ID

    Returns:
        Target entity ID, or 0 if no target
    """
    ...


def set_target(entity_id: int, target_id: int) -> None:
    """
    Set the current target of an entity.

    Args:
        entity_id: The entity ID
        target_id: The target entity ID
    """
    ...


# =============================================================================
# Buffs and Debuffs
# =============================================================================

def apply_buff(entity_id: int, buff_name: str, buff_data: Dict[str, Any]) -> bool:
    """
    Apply a buff to an entity.

    Args:
        entity_id: The entity ID
        buff_name: Unique buff identifier
        buff_data: Dictionary with buff parameters (duration, stats, etc.)

    Returns:
        True if buff was applied
    """
    ...


def apply_debuff(entity_id: int, debuff_name: str, debuff_data: Dict[str, Any]) -> bool:
    """
    Apply a debuff to an entity.

    Args:
        entity_id: The entity ID
        debuff_name: Unique debuff identifier
        debuff_data: Dictionary with debuff parameters

    Returns:
        True if debuff was applied
    """
    ...


def has_buff(entity_id: int, buff_name: str) -> bool:
    """
    Check if an entity has a specific buff.

    Args:
        entity_id: The entity ID
        buff_name: The buff identifier

    Returns:
        True if entity has the buff
    """
    ...


def has_debuff(entity_id: int, debuff_name: str) -> bool:
    """
    Check if an entity has a specific debuff.

    Args:
        entity_id: The entity ID
        debuff_name: The debuff identifier

    Returns:
        True if entity has the debuff
    """
    ...


def get_buff(entity_id: int, buff_name: str) -> Optional[Dict[str, Any]]:
    """
    Get buff data from an entity.

    Args:
        entity_id: The entity ID
        buff_name: The buff identifier

    Returns:
        Buff data dictionary, or None if not found
    """
    ...


def get_debuff(entity_id: int, debuff_name: str) -> Optional[Dict[str, Any]]:
    """
    Get debuff data from an entity.

    Args:
        entity_id: The entity ID
        debuff_name: The debuff identifier

    Returns:
        Debuff data dictionary, or None if not found
    """
    ...


def remove_buff(entity_id: int, buff_name: str) -> bool:
    """
    Remove a buff from an entity.

    Args:
        entity_id: The entity ID
        buff_name: The buff identifier

    Returns:
        True if buff was removed
    """
    ...


def remove_debuff(entity_id: int, debuff_name: str) -> bool:
    """
    Remove a debuff from an entity.

    Args:
        entity_id: The entity ID
        debuff_name: The debuff identifier

    Returns:
        True if debuff was removed
    """
    ...


def set_debuff_duration(entity_id: int, debuff_name: str, duration: float) -> None:
    """
    Set/refresh the duration of a debuff.

    Args:
        entity_id: The entity ID
        debuff_name: The debuff identifier
        duration: New duration in seconds
    """
    ...


# =============================================================================
# Query Functions
# =============================================================================

def find_entities_in_radius(x: float, y: float, z: float, radius: float) -> List[int]:
    """
    Find all entities within a radius of a point.

    Args:
        x: Center X coordinate
        y: Center Y coordinate
        z: Center Z coordinate
        radius: Search radius

    Returns:
        List of entity IDs within the radius
    """
    ...


def find_allies_in_radius(entity_id: int, x: float, y: float, z: float, radius: float) -> List[int]:
    """
    Find all allied entities within a radius.

    Args:
        entity_id: Reference entity for alliance check
        x: Center X coordinate
        y: Center Y coordinate
        z: Center Z coordinate
        radius: Search radius

    Returns:
        List of allied entity IDs within the radius
    """
    ...


def find_enemies_in_radius(entity_id: int, x: float, y: float, z: float, radius: float) -> List[int]:
    """
    Find all enemy entities within a radius.

    Args:
        entity_id: Reference entity for enemy check
        x: Center X coordinate
        y: Center Y coordinate
        z: Center Z coordinate
        radius: Search radius

    Returns:
        List of enemy entity IDs within the radius
    """
    ...


def distance_to(entity_id: int, other_id: int) -> float:
    """
    Calculate distance between two entities.

    Args:
        entity_id: First entity ID
        other_id: Second entity ID

    Returns:
        Distance in world units
    """
    ...


def distance_3d(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float) -> float:
    """
    Calculate 3D distance between two points.

    Args:
        x1, y1, z1: First point coordinates
        x2, y2, z2: Second point coordinates

    Returns:
        Distance in world units
    """
    ...


def has_line_of_sight(entity_id: int, target_id: int) -> bool:
    """
    Check if there is line of sight between two entities.

    Args:
        entity_id: Source entity ID
        target_id: Target entity ID

    Returns:
        True if there is clear line of sight
    """
    ...


def raycast(x: float, y: float, z: float, dx: float, dy: float, dz: float, max_dist: float) -> Optional[Tuple[int, float, float, float]]:
    """
    Cast a ray and find first hit.

    Args:
        x, y, z: Ray origin
        dx, dy, dz: Ray direction (normalized)
        max_dist: Maximum ray distance

    Returns:
        Tuple of (entity_id, hit_x, hit_y, hit_z) or None if no hit
    """
    ...


# =============================================================================
# Properties
# =============================================================================

def get_property(entity_id: int, name: str) -> Any:
    """
    Get a custom property from an entity.

    Args:
        entity_id: The entity ID
        name: Property name

    Returns:
        Property value, or None if not found
    """
    ...


def set_property(entity_id: int, name: str, value: Any) -> None:
    """
    Set a custom property on an entity.

    Args:
        entity_id: The entity ID
        name: Property name
        value: Property value
    """
    ...


def get_stat(entity_id: int, stat_name: str) -> float:
    """
    Get a stat value from an entity.

    Args:
        entity_id: The entity ID
        stat_name: Stat name (e.g., "attack_damage", "armor")

    Returns:
        Stat value
    """
    ...


def set_stat(entity_id: int, stat_name: str, value: float) -> None:
    """
    Set a stat value on an entity.

    Args:
        entity_id: The entity ID
        stat_name: Stat name
        value: New stat value
    """
    ...


# =============================================================================
# Audio and Visual Effects
# =============================================================================

def play_sound(name: str, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None:
    """
    Play a sound effect at a position.

    Args:
        name: Sound effect name/path
        x: X coordinate (optional, 0 for 2D sound)
        y: Y coordinate
        z: Z coordinate
    """
    ...


def play_ui_sound(name: str) -> None:
    """
    Play a UI sound effect (non-positional).

    Args:
        name: Sound effect name/path
    """
    ...


def play_effect(name: str, x: float, y: float, z: float) -> int:
    """
    Play a visual effect at a position.

    Args:
        name: Effect name/path
        x: X coordinate
        y: Y coordinate
        z: Z coordinate

    Returns:
        Effect instance ID
    """
    ...


def stop_effect(effect_id: int) -> None:
    """
    Stop a playing visual effect.

    Args:
        effect_id: The effect instance ID
    """
    ...


def play_animation(entity_id: int, animation_name: str) -> None:
    """
    Play an animation on an entity.

    Args:
        entity_id: The entity ID
        animation_name: Name of the animation to play
    """
    ...


def spawn_damage_number(x: float, y: float, z: float, amount: int, damage_type: str) -> None:
    """
    Spawn a floating damage number.

    Args:
        x: X coordinate
        y: Y coordinate
        z: Z coordinate
        amount: Damage amount to display
        damage_type: Type for color coding (e.g., "physical", "fire")
    """
    ...


def spawn_floating_text(x: float, y: float, z: float, text: str, style: str) -> None:
    """
    Spawn floating text at a position.

    Args:
        x: X coordinate
        y: Y coordinate
        z: Z coordinate
        text: Text to display
        style: Style preset (e.g., "experience", "level_up")
    """
    ...


def shake_camera_in_radius(x: float, y: float, z: float, radius: float, intensity: float, duration: float) -> None:
    """
    Apply camera shake to players within radius.

    Args:
        x: Center X coordinate
        y: Center Y coordinate
        z: Center Z coordinate
        radius: Effect radius
        intensity: Shake intensity (0.0 to 1.0)
        duration: Shake duration in seconds
    """
    ...


def copy_visual(source_id: int, target_id: int) -> None:
    """
    Copy visual properties from one entity to another.

    Args:
        source_id: Source entity ID
        target_id: Target entity ID
    """
    ...


# =============================================================================
# Projectiles
# =============================================================================

def spawn_projectile(projectile_type: str, start_x: float, start_y: float, start_z: float,
                     target_x: float, target_y: float, target_z: float, owner_id: int) -> int:
    """
    Spawn a projectile traveling from start to target.

    Args:
        projectile_type: Type of projectile
        start_x, start_y, start_z: Starting position
        target_x, target_y, target_z: Target position
        owner_id: Entity that owns/fired the projectile

    Returns:
        Projectile entity ID
    """
    ...


# =============================================================================
# UI Functions
# =============================================================================

def show_notification(player_id: int, message: str, notification_type: str = "info") -> None:
    """
    Show a notification to a player.

    Args:
        player_id: The player ID
        message: Notification message
        notification_type: Type ("info", "success", "error", "warning")
    """
    ...


def show_error_message(entity_id: int, message: str) -> None:
    """
    Show an error message above an entity.

    Args:
        entity_id: The entity ID
        message: Error message
    """
    ...


# =============================================================================
# Resources and Economy
# =============================================================================

def get_resource(player_id: int, resource: str) -> int:
    """
    Get a player's resource amount.

    Args:
        player_id: The player ID
        resource: Resource type (e.g., "gold", "wood", "food")

    Returns:
        Resource amount
    """
    ...


def modify_resource(player_id: int, resource: str, amount: int) -> None:
    """
    Modify a player's resource amount.

    Args:
        player_id: The player ID
        resource: Resource type
        amount: Amount to add (negative to subtract)
    """
    ...


def get_population(player_id: int) -> int:
    """
    Get a player's current population.

    Args:
        player_id: The player ID

    Returns:
        Current population count
    """
    ...


def get_max_population(player_id: int) -> int:
    """
    Get a player's population cap.

    Args:
        player_id: The player ID

    Returns:
        Maximum population
    """
    ...


def modify_reserved_population(player_id: int, amount: int) -> None:
    """
    Modify reserved population (for units in production).

    Args:
        player_id: The player ID
        amount: Amount to reserve (negative to release)
    """
    ...


# =============================================================================
# Technology
# =============================================================================

def has_tech(player_id: int, tech_id: str) -> bool:
    """
    Check if a player has researched a technology.

    Args:
        player_id: The player ID
        tech_id: Technology identifier

    Returns:
        True if technology is researched
    """
    ...


def grant_tech(player_id: int, tech_id: str) -> None:
    """
    Grant a technology to a player.

    Args:
        player_id: The player ID
        tech_id: Technology identifier
    """
    ...


def is_researching(player_id: int, tech_id: str) -> bool:
    """
    Check if a player is currently researching a technology.

    Args:
        player_id: The player ID
        tech_id: Technology identifier

    Returns:
        True if currently researching
    """
    ...


def begin_research(player_id: int, tech_id: str, duration: float) -> None:
    """
    Start researching a technology.

    Args:
        player_id: The player ID
        tech_id: Technology identifier
        duration: Research duration in seconds
    """
    ...


def get_research_elapsed(player_id: int, tech_id: str) -> float:
    """
    Get elapsed research time for a technology.

    Args:
        player_id: The player ID
        tech_id: Technology identifier

    Returns:
        Elapsed time in seconds
    """
    ...


def get_tech_name(tech_id: str) -> str:
    """
    Get the display name of a technology.

    Args:
        tech_id: Technology identifier

    Returns:
        Technology display name
    """
    ...


# =============================================================================
# Buildings
# =============================================================================

def player_has_building(player_id: int, building_type: str) -> bool:
    """
    Check if a player has at least one building of a type.

    Args:
        player_id: The player ID
        building_type: Building type identifier

    Returns:
        True if player has the building
    """
    ...


def get_main_building(player_id: int) -> int:
    """
    Get a player's main building (town center, etc.).

    Args:
        player_id: The player ID

    Returns:
        Building entity ID, or 0 if none
    """
    ...


def unlock_building_type(player_id: int, building_type: str) -> None:
    """
    Unlock a building type for a player.

    Args:
        player_id: The player ID
        building_type: Building type identifier
    """
    ...


# =============================================================================
# Units
# =============================================================================

def unlock_unit_type(player_id: int, unit_type: str) -> None:
    """
    Unlock a unit type for a player.

    Args:
        player_id: The player ID
        unit_type: Unit type identifier
    """
    ...


def get_player_units(player_id: int) -> List[int]:
    """
    Get all units owned by a player.

    Args:
        player_id: The player ID

    Returns:
        List of unit entity IDs
    """
    ...


def count_player_units(player_id: int, unit_type: str) -> int:
    """
    Count units of a specific type owned by a player.

    Args:
        player_id: The player ID
        unit_type: Unit type to count (empty for all)

    Returns:
        Number of matching units
    """
    ...


def apply_unit_upgrade(unit_id: int, upgrade_data: Dict[str, Any]) -> None:
    """
    Apply an upgrade to a unit.

    Args:
        unit_id: The unit entity ID
        upgrade_data: Dictionary of stats to modify
    """
    ...


def apply_global_stat_bonus(player_id: int, stat: str, bonus: float) -> None:
    """
    Apply a stat bonus to all of a player's units.

    Args:
        player_id: The player ID
        stat: Stat name
        bonus: Bonus value
    """
    ...


def apply_resource_multiplier(player_id: int, resource: str, multiplier: float) -> None:
    """
    Apply a resource gathering multiplier.

    Args:
        player_id: The player ID
        resource: Resource type
        multiplier: Multiplier value (1.0 = normal)
    """
    ...


# =============================================================================
# Player Functions
# =============================================================================

def is_player(entity_id: int) -> bool:
    """
    Check if an entity is a player character.

    Args:
        entity_id: The entity ID

    Returns:
        True if entity is a player
    """
    ...


def get_player_age(player_id: int) -> int:
    """
    Get a player's current age/era level.

    Args:
        player_id: The player ID

    Returns:
        Age level (1, 2, 3, etc.)
    """
    ...


def grant_experience(entity_id: int, amount: int) -> None:
    """
    Grant experience points to an entity.

    Args:
        entity_id: The entity ID
        amount: XP amount
    """
    ...


def increment_stat(player_id: int, stat_name: str) -> None:
    """
    Increment a player statistic counter.

    Args:
        player_id: The player ID
        stat_name: Statistic name
    """
    ...


def check_achievements(player_id: int, trigger: str) -> None:
    """
    Check and potentially unlock achievements.

    Args:
        player_id: The player ID
        trigger: Achievement trigger string
    """
    ...


def unlock_ability(player_id: int, ability_id: str) -> None:
    """
    Unlock an ability for a player.

    Args:
        player_id: The player ID
        ability_id: Ability identifier
    """
    ...


# =============================================================================
# Loot and Items
# =============================================================================

def generate_loot(loot_table: str) -> List[str]:
    """
    Generate loot from a loot table.

    Args:
        loot_table: Loot table identifier

    Returns:
        List of item identifiers
    """
    ...


def drop_item(item_id: str, x: float, y: float, z: float) -> int:
    """
    Drop an item at a position.

    Args:
        item_id: Item identifier
        x, y, z: Drop position

    Returns:
        Dropped item entity ID
    """
    ...


# =============================================================================
# Events
# =============================================================================

def trigger_event(entity_id: int, event_name: str, event_data: Dict[str, Any]) -> None:
    """
    Trigger a custom event on an entity.

    Args:
        entity_id: The entity ID
        event_name: Event name
        event_data: Event data dictionary
    """
    ...


def trigger_game_event(event_name: str, event_data: Dict[str, Any]) -> None:
    """
    Trigger a global game event.

    Args:
        event_name: Event name
        event_data: Event data dictionary
    """
    ...


# =============================================================================
# Channeling
# =============================================================================

def start_channel(entity_id: int, channel_name: str, duration: float, channel_data: Dict[str, Any]) -> None:
    """
    Start a channeled ability.

    Args:
        entity_id: The channeling entity
        channel_name: Channel identifier
        duration: Channel duration in seconds
        channel_data: Data to store with channel
    """
    ...


def is_channeling(entity_id: int) -> bool:
    """
    Check if an entity is currently channeling.

    Args:
        entity_id: The entity ID

    Returns:
        True if channeling
    """
    ...


def interrupt_channel(entity_id: int) -> None:
    """
    Interrupt an entity's current channel.

    Args:
        entity_id: The entity ID
    """
    ...


# =============================================================================
# Global State
# =============================================================================

def get_time() -> float:
    """
    Get current game time in seconds.

    Returns:
        Game time in seconds since start
    """
    ...


def get_game_time() -> float:
    """
    Get total game time (may differ from get_time in paused state).

    Returns:
        Total game time in seconds
    """
    ...


def is_daytime() -> bool:
    """
    Check if it's currently daytime in the game world.

    Returns:
        True if daytime
    """
    ...


def get_weather() -> str:
    """
    Get current weather condition.

    Returns:
        Weather string (e.g., "clear", "rain", "storm")
    """
    ...


def get_global_variable(name: str) -> Any:
    """
    Get a global game variable.

    Args:
        name: Variable name

    Returns:
        Variable value, or None if not found
    """
    ...


def set_global_variable(name: str, value: Any) -> None:
    """
    Set a global game variable.

    Args:
        name: Variable name
        value: Variable value
    """
    ...


# =============================================================================
# Utility Functions
# =============================================================================

def random_range(min_val: float, max_val: float) -> float:
    """
    Get a random float in the specified range.

    Args:
        min_val: Minimum value (inclusive)
        max_val: Maximum value (inclusive)

    Returns:
        Random value between min and max
    """
    ...


def random_chance(probability: float) -> bool:
    """
    Return True with the given probability.

    Args:
        probability: Probability from 0.0 to 1.0

    Returns:
        True with given probability
    """
    ...


def random_int(min_val: int, max_val: int) -> int:
    """
    Get a random integer in the specified range.

    Args:
        min_val: Minimum value (inclusive)
        max_val: Maximum value (inclusive)

    Returns:
        Random integer between min and max
    """
    ...


def clamp(value: float, min_val: float, max_val: float) -> float:
    """
    Clamp a value between min and max.

    Args:
        value: Value to clamp
        min_val: Minimum value
        max_val: Maximum value

    Returns:
        Clamped value
    """
    ...


def lerp(a: float, b: float, t: float) -> float:
    """
    Linear interpolation between two values.

    Args:
        a: Start value
        b: End value
        t: Interpolation factor (0.0 to 1.0)

    Returns:
        Interpolated value
    """
    ...


# =============================================================================
# Condition Evaluation
# =============================================================================

def evaluate_condition(condition_id: str, context: Dict[str, Any]) -> Tuple[int, str]:
    """
    Evaluate a registered condition.

    Args:
        condition_id: Condition identifier
        context: Evaluation context

    Returns:
        Tuple of (result_code, explanation)
        result_code: 0=False, 1=True, 2=Unknown
    """
    ...


# =============================================================================
# Debugging
# =============================================================================

def log_debug(message: str) -> None:
    """
    Log a debug message.

    Args:
        message: Message to log
    """
    ...


def log_info(message: str) -> None:
    """
    Log an info message.

    Args:
        message: Message to log
    """
    ...


def log_warning(message: str) -> None:
    """
    Log a warning message.

    Args:
        message: Message to log
    """
    ...


def log_error(message: str) -> None:
    """
    Log an error message.

    Args:
        message: Message to log
    """
    ...
