"""
Building Complete Event Handler for Vehement2

Handles the BuildingComplete event:
- Play celebration effects
- Grant resource bonuses
- Update game statistics
- Trigger achievements

This script demonstrates:
- Event handling in Python
- Interacting with game systems
- Playing effects and notifications
"""

import nova_engine as nova

# ============================================================================
# Constants
# ============================================================================

# Resource bonuses for completing buildings
BUILDING_BONUSES = {
    "CommandCenter": {"Coins": 100, "Food": 50},
    "Farm": {"Food": 25},
    "LumberMill": {"Wood": 25},
    "Quarry": {"Stone": 25},
    "Workshop": {"Metal": 10},
    "WatchTower": {"Coins": 15},
    "Wall": {},
    "Gate": {},
    "Fortress": {"Coins": 200, "Metal": 25},
    "TradingPost": {"Coins": 50},
    "Hospital": {"Medicine": 10},
    "Warehouse": {"Coins": 25},
    "Shelter": {},
    "House": {},
    "Barracks": {"Ammunition": 20},
}

# Special effects for different building types
BUILDING_EFFECTS = {
    "CommandCenter": "major_celebration",
    "Fortress": "major_celebration",
    "Farm": "minor_celebration",
    "LumberMill": "minor_celebration",
    "Quarry": "minor_celebration",
    "Workshop": "construction_complete",
    "WatchTower": "construction_complete",
    "Wall": None,
    "Gate": "construction_complete",
    "TradingPost": "trade_established",
    "Hospital": "healing_ready",
    "Warehouse": "construction_complete",
    "Shelter": "construction_complete",
    "House": "construction_complete",
    "Barracks": "military_ready",
}

# Sound effects
BUILDING_SOUNDS = {
    "CommandCenter": "fanfare_major",
    "Fortress": "fanfare_major",
    "Farm": "building_complete",
    "LumberMill": "building_complete",
    "Quarry": "building_complete",
    "Workshop": "building_complete",
    "WatchTower": "tower_complete",
    "Wall": "wall_complete",
    "Gate": "gate_complete",
    "TradingPost": "trade_horn",
    "Hospital": "building_complete",
    "Warehouse": "building_complete",
    "Shelter": "building_complete",
    "House": "building_complete",
    "Barracks": "military_horn",
}

# Achievement tracking
buildings_completed = {}

# ============================================================================
# Main Event Handler
# ============================================================================

def handle_building_complete(event_type, entity_id, target_entity_id, building_id,
                             x, y, z, tile_x, tile_y, float_value, int_value,
                             string_value):
    """
    Main event handler for building completion.

    Called by the engine when a building finishes construction.

    Args:
        event_type: The event type ID
        entity_id: Usually 0 for building events
        target_entity_id: Usually 0
        building_id: The completed building's ID
        x, y, z: World position of the building
        tile_x, tile_y: Tile coordinates
        float_value: Construction time taken
        int_value: Building level
        string_value: Building type name

    Returns:
        False to allow event propagation, True to cancel
    """
    building_type = string_value
    building_level = int_value
    construction_time = float_value

    nova.log_info(f"Building completed: {building_type} (ID: {building_id}) at ({tile_x}, {tile_y})")

    # Play sound effect
    play_completion_sound(building_type, x, y, z)

    # Spawn visual effect
    spawn_completion_effect(building_type, x, y, z)

    # Grant resource bonuses
    grant_bonuses(building_type, building_level)

    # Show notification
    show_completion_notification(building_type, building_level)

    # Track statistics
    track_building_completion(building_type, construction_time)

    # Check achievements
    check_achievements(building_type)

    # Don't cancel the event - let other handlers process it
    return False

# ============================================================================
# Helper Functions
# ============================================================================

def play_completion_sound(building_type, x, y, z):
    """Play the appropriate sound for building completion."""
    sound = BUILDING_SOUNDS.get(building_type, "building_complete")
    if sound:
        nova.play_sound(sound, x, y, z)

def spawn_completion_effect(building_type, x, y, z):
    """Spawn visual effects for building completion."""
    effect = BUILDING_EFFECTS.get(building_type)

    if effect == "major_celebration":
        # Big fireworks display
        nova.spawn_particles("fireworks", x, y + 5, z, 50)
        nova.spawn_particles("confetti", x, y + 2, z, 100)
        nova.spawn_effect("celebration_ring", x, y, z)

    elif effect == "minor_celebration":
        # Small celebration
        nova.spawn_particles("confetti", x, y + 2, z, 30)
        nova.spawn_effect("construction_dust", x, y, z)

    elif effect == "construction_complete":
        # Standard completion effect
        nova.spawn_particles("dust", x, y, z, 20)
        nova.spawn_effect("construction_complete", x, y, z)

    elif effect == "trade_established":
        # Trading post effect
        nova.spawn_particles("gold_sparkle", x, y + 1, z, 25)
        nova.spawn_effect("trade_banner", x, y, z)

    elif effect == "healing_ready":
        # Hospital effect
        nova.spawn_particles("healing_glow", x, y + 1, z, 20)
        nova.spawn_effect("medical_cross", x, y + 2, z)

    elif effect == "military_ready":
        # Barracks effect
        nova.spawn_particles("dust", x, y, z, 15)
        nova.spawn_effect("military_banner", x, y, z)

def grant_bonuses(building_type, building_level):
    """Grant resource bonuses for completing the building."""
    bonuses = BUILDING_BONUSES.get(building_type, {})

    # Scale bonuses with building level
    level_multiplier = 1.0 + (building_level - 1) * 0.5

    for resource, amount in bonuses.items():
        bonus_amount = int(amount * level_multiplier)
        if bonus_amount > 0:
            nova.add_resource(resource, bonus_amount)
            nova.log_info(f"Granted {bonus_amount} {resource} as construction bonus")

def show_completion_notification(building_type, building_level):
    """Show a notification about the completed building."""
    level_str = ""
    if building_level > 1:
        level_str = f" (Level {building_level})"

    message = f"{building_type}{level_str} construction complete!"

    # Different notification styles based on importance
    if building_type in ["CommandCenter", "Fortress"]:
        nova.show_notification(message, 5.0)
    else:
        nova.show_notification(message, 3.0)

    # Show bonus notification if applicable
    bonuses = BUILDING_BONUSES.get(building_type, {})
    if bonuses:
        bonus_str = ", ".join([f"+{amt} {res}" for res, amt in bonuses.items()])
        nova.show_notification(f"Bonus: {bonus_str}", 2.0)

def track_building_completion(building_type, construction_time):
    """Track building completion statistics."""
    global buildings_completed

    if building_type not in buildings_completed:
        buildings_completed[building_type] = {
            "count": 0,
            "total_time": 0.0,
            "fastest": float('inf'),
        }

    stats = buildings_completed[building_type]
    stats["count"] += 1
    stats["total_time"] += construction_time

    if construction_time < stats["fastest"]:
        stats["fastest"] = construction_time
        nova.log_info(f"New fastest construction time for {building_type}: {construction_time:.1f}s")

def check_achievements(building_type):
    """Check and trigger achievements based on building completion."""
    global buildings_completed

    total_buildings = sum(stats["count"] for stats in buildings_completed.values())

    # First building achievement
    if total_buildings == 1:
        trigger_achievement("First Steps", "Complete your first building")

    # 10 buildings achievement
    if total_buildings == 10:
        trigger_achievement("Builder", "Complete 10 buildings")

    # 50 buildings achievement
    if total_buildings == 50:
        trigger_achievement("Architect", "Complete 50 buildings")

    # Type-specific achievements
    type_count = buildings_completed.get(building_type, {}).get("count", 0)

    if building_type == "Farm" and type_count == 5:
        trigger_achievement("Farmer", "Build 5 farms")

    if building_type == "WatchTower" and type_count == 10:
        trigger_achievement("Watchtower Network", "Build 10 watch towers")

    if building_type == "Fortress" and type_count == 1:
        trigger_achievement("Stronghold", "Build your first fortress")

    # Special combo achievements
    if all(bt in buildings_completed for bt in ["Farm", "LumberMill", "Quarry"]):
        if all(buildings_completed[bt]["count"] >= 1 for bt in ["Farm", "LumberMill", "Quarry"]):
            trigger_achievement("Self-Sufficient", "Build a farm, lumber mill, and quarry")

def trigger_achievement(name, description):
    """Trigger an achievement unlock."""
    nova.show_notification(f"Achievement Unlocked: {name}", 5.0)
    nova.play_sound("achievement_unlock", 0, 0, 0)
    nova.spawn_effect("achievement_glow", 0, 0, 0)
    nova.log_info(f"Achievement unlocked: {name} - {description}")

# ============================================================================
# Additional Event Handlers
# ============================================================================

def on_building_upgraded(event_type, entity_id, target_entity_id, building_id,
                         x, y, z, tile_x, tile_y, float_value, int_value,
                         string_value):
    """Handle building upgrade completion."""
    building_type = string_value
    new_level = int_value

    nova.log_info(f"Building upgraded: {building_type} to level {new_level}")

    # Play upgrade effects
    nova.play_sound("upgrade_complete", x, y, z)
    nova.spawn_particles("upgrade_sparkle", x, y + 2, z, 30)
    nova.spawn_effect("level_up_ring", x, y, z)

    # Grant upgrade bonuses (smaller than construction)
    bonuses = BUILDING_BONUSES.get(building_type, {})
    for resource, amount in bonuses.items():
        bonus_amount = int(amount * 0.5)  # Half bonus for upgrades
        if bonus_amount > 0:
            nova.add_resource(resource, bonus_amount)

    nova.show_notification(f"{building_type} upgraded to Level {new_level}!", 3.0)

    return False

def on_building_destroyed(event_type, entity_id, target_entity_id, building_id,
                          x, y, z, tile_x, tile_y, float_value, int_value,
                          string_value):
    """Handle building destruction."""
    building_type = string_value

    nova.log_warning(f"Building destroyed: {building_type} at ({tile_x}, {tile_y})")

    # Play destruction effects
    nova.play_sound("building_collapse", x, y, z)
    nova.spawn_particles("debris", x, y, z, 50)
    nova.spawn_particles("dust_cloud", x, y, z, 30)
    nova.spawn_effect("collapse_smoke", x, y, z)

    # Show warning notification
    nova.show_warning(f"{building_type} has been destroyed!")

    return False

# ============================================================================
# Initialization
# ============================================================================

def init():
    """Initialize the event handler module."""
    nova.log_info("Building event handlers initialized")
    return "success"
