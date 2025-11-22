"""
Fireball Spell Effect Example Script
Demonstrates a classic fireball spell with projectile, impact, and area damage.

This example shows:
- Spell casting with mana cost and cooldown
- Projectile spawning and tracking
- Area of effect damage with falloff
- Visual and audio effects
- Damage over time (burning)
"""

from game_api import *
from typing import Tuple, List, Optional
import math


# Spell Configuration
SPELL_NAME = "Fireball"
SPELL_ID = "spell_fireball"
MANA_COST = 35
COOLDOWN = 3.0  # seconds
CAST_TIME = 0.5  # seconds (instant for this example)
RANGE = 25.0

# Damage Configuration
BASE_DAMAGE = 80.0
SPELL_POWER_SCALING = 0.8  # 80% of spell power added to damage
AOE_RADIUS = 4.0
AOE_FALLOFF = 0.3  # 30% damage reduction per unit distance from center
MIN_DAMAGE_PERCENT = 0.25  # Minimum 25% damage at edge of AOE

# Burn Effect
BURN_DURATION = 4.0
BURN_DAMAGE_PER_SECOND = 15.0
BURN_TICK_INTERVAL = 1.0

# Projectile
PROJECTILE_SPEED = 20.0
PROJECTILE_TYPE = "fireball_projectile"


def can_cast_fireball(caster_id: int, target_pos: Tuple[float, float, float] = None) -> Tuple[bool, str]:
    """
    Check if the caster can cast Fireball.

    Args:
        caster_id: The entity attempting to cast
        target_pos: The target position (optional)

    Returns:
        Tuple of (can_cast, reason_if_not)
    """
    # Check if caster is alive
    if not is_alive(caster_id):
        return False, "Caster is dead"

    # Check mana
    mana = get_mana(caster_id)
    if mana < MANA_COST:
        return False, f"Not enough mana ({mana}/{MANA_COST})"

    # Check cooldown
    last_cast = get_property(caster_id, f"cooldown_{SPELL_ID}")
    if last_cast:
        elapsed = get_time() - last_cast
        if elapsed < COOLDOWN:
            remaining = COOLDOWN - elapsed
            return False, f"On cooldown ({remaining:.1f}s)"

    # Check if silenced or stunned
    if has_debuff(caster_id, "silence") or has_debuff(caster_id, "stun"):
        return False, "Cannot cast while silenced or stunned"

    # Check range if target specified
    if target_pos:
        cx, cy, cz = get_position(caster_id)
        tx, ty, tz = target_pos
        dist = distance_3d(cx, cy, cz, tx, ty, tz)
        if dist > RANGE:
            return False, f"Target out of range ({dist:.1f}/{RANGE})"

    return True, ""


def cast_fireball(caster_id: int, target_pos: Tuple[float, float, float]) -> bool:
    """
    Cast the Fireball spell.

    Args:
        caster_id: The entity casting the spell
        target_pos: The target position (x, y, z)

    Returns:
        True if the spell was cast successfully
    """
    # Validate casting
    can_cast, reason = can_cast_fireball(caster_id, target_pos)
    if not can_cast:
        show_error_message(caster_id, f"Cannot cast {SPELL_NAME}: {reason}")
        return False

    # Get caster position
    cx, cy, cz = get_position(caster_id)
    tx, ty, tz = target_pos

    # Consume mana
    current_mana = get_mana(caster_id)
    set_mana(caster_id, current_mana - MANA_COST)

    # Start cooldown
    set_property(caster_id, f"cooldown_{SPELL_ID}", get_time())

    # Face target
    look_at(caster_id, tx, ty, tz)

    # Play cast animation
    play_animation(caster_id, "cast_spell")

    # Play cast sound and effect
    play_sound("fireball_cast", cx, cy, cz)
    play_effect("fire_charge", cx, cy + 1.5, cz)

    # Spawn projectile
    # Start slightly in front of caster at chest height
    start_y = cy + 1.5

    projectile_id = spawn_projectile(
        PROJECTILE_TYPE,
        cx, start_y, cz,
        tx, ty, tz,
        caster_id
    )

    # Store spell data on projectile
    spell_data = {
        "spell_id": SPELL_ID,
        "caster_id": caster_id,
        "base_damage": calculate_damage(caster_id),
        "target_pos": target_pos,
    }
    set_property(projectile_id, "spell_data", spell_data)
    set_property(projectile_id, "speed", PROJECTILE_SPEED)

    log_debug(f"Entity {caster_id} cast {SPELL_NAME} towards ({tx}, {ty}, {tz})")
    return True


def calculate_damage(caster_id: int) -> float:
    """
    Calculate the fireball's damage based on caster stats.

    Args:
        caster_id: The casting entity

    Returns:
        The calculated damage value
    """
    damage_value = BASE_DAMAGE

    # Add spell power scaling
    spell_power = get_property(caster_id, "spell_power")
    if spell_power:
        damage_value += spell_power * SPELL_POWER_SCALING

    # Check for fire damage bonuses
    fire_bonus = get_property(caster_id, "fire_damage_bonus")
    if fire_bonus:
        damage_value *= (1.0 + fire_bonus)

    # Check for critical hit
    crit_chance = get_property(caster_id, "spell_crit_chance")
    if crit_chance and random_chance(crit_chance):
        crit_mult = get_property(caster_id, "spell_crit_multiplier")
        if not crit_mult:
            crit_mult = 1.5
        damage_value *= crit_mult
        log_debug(f"{SPELL_NAME} critical hit!")

    return damage_value


def on_fireball_impact(projectile_id: int, hit_pos: Tuple[float, float, float], hit_entity: int = 0) -> None:
    """
    Called when the fireball projectile impacts something.

    Args:
        projectile_id: The projectile entity
        hit_pos: The impact position
        hit_entity: The entity that was directly hit (0 if ground)
    """
    # Get spell data
    spell_data = get_property(projectile_id, "spell_data")
    if not spell_data or spell_data.get("spell_id") != SPELL_ID:
        return

    caster_id = spell_data["caster_id"]
    base_damage = spell_data["base_damage"]

    x, y, z = hit_pos

    # Play impact effects
    play_effect("fireball_explosion", x, y, z)
    play_sound("fireball_impact", x, y, z)

    # Screen shake for nearby players
    shake_camera_in_radius(x, y, z, AOE_RADIUS * 2, intensity=0.3, duration=0.2)

    # Find all entities in AOE
    nearby = find_entities_in_radius(x, y, z, AOE_RADIUS)

    # Filter to valid targets (enemies of caster, alive)
    targets = []
    for entity in nearby:
        if entity == caster_id:
            continue  # Don't damage self
        if not is_alive(entity):
            continue
        if not is_enemy(caster_id, entity):
            continue  # Only damage enemies
        targets.append(entity)

    log_debug(f"{SPELL_NAME} impact at ({x}, {y}, {z}), {len(targets)} targets in range")

    # Apply damage to all targets with distance falloff
    for target_id in targets:
        apply_fireball_damage(caster_id, target_id, base_damage, x, y, z)


def apply_fireball_damage(caster_id: int, target_id: int, base_damage: float,
                          impact_x: float, impact_y: float, impact_z: float) -> None:
    """
    Apply fireball damage to a single target.

    Args:
        caster_id: The caster entity
        target_id: The target entity
        base_damage: The base damage amount
        impact_x/y/z: The impact center position
    """
    # Get target position
    tx, ty, tz = get_position(target_id)

    # Calculate distance from impact center
    dist = distance_3d(impact_x, impact_y, impact_z, tx, ty, tz)

    # Calculate damage falloff based on distance
    if dist <= 1.0:
        # Full damage at center
        damage_mult = 1.0
    else:
        # Linear falloff with distance
        falloff = (dist - 1.0) * AOE_FALLOFF
        damage_mult = max(MIN_DAMAGE_PERCENT, 1.0 - falloff)

    final_damage = base_damage * damage_mult

    # Check fire resistance
    fire_resist = get_property(target_id, "fire_resistance")
    if fire_resist:
        final_damage *= (1.0 - min(0.75, fire_resist))  # Cap at 75% resistance

    # Apply damage
    damage(target_id, final_damage, "magical", caster_id)

    # Show damage number
    spawn_damage_number(tx, ty + 2.0, tz, int(final_damage), "fire")

    # Apply burn debuff
    apply_burn_debuff(caster_id, target_id)

    log_debug(f"{SPELL_NAME} dealt {final_damage:.1f} damage to entity {target_id}")


def apply_burn_debuff(caster_id: int, target_id: int) -> None:
    """
    Apply the burning damage over time effect.

    Args:
        caster_id: The caster entity
        target_id: The target to burn
    """
    # Check if target is already burning (refresh duration)
    existing_burn = get_debuff(target_id, "burning")

    burn_data = {
        "caster_id": caster_id,
        "damage_per_tick": BURN_DAMAGE_PER_SECOND * BURN_TICK_INTERVAL,
        "tick_interval": BURN_TICK_INTERVAL,
        "duration": BURN_DURATION,
        "damage_type": "magical",
        "effect": "burning_aura",
    }

    if existing_burn:
        # Refresh duration
        set_debuff_duration(target_id, "burning", BURN_DURATION)
        log_debug(f"Refreshed burning on entity {target_id}")
    else:
        # Apply new debuff
        apply_debuff(target_id, "burning", burn_data)
        play_effect("ignite", *get_position(target_id))
        log_debug(f"Applied burning to entity {target_id}")


def on_burn_tick(target_id: int, debuff_data: dict) -> None:
    """
    Called each tick of the burning debuff.

    Args:
        target_id: The burning entity
        debuff_data: The debuff's stored data
    """
    if not is_alive(target_id):
        return

    caster_id = debuff_data.get("caster_id", 0)
    tick_damage = debuff_data.get("damage_per_tick", BURN_DAMAGE_PER_SECOND)

    # Check fire resistance
    fire_resist = get_property(target_id, "fire_resistance")
    if fire_resist:
        tick_damage *= (1.0 - min(0.75, fire_resist))

    # Apply damage
    damage(target_id, tick_damage, "magical", caster_id)

    # Visual effect
    x, y, z = get_position(target_id)
    play_effect("burn_tick", x, y + 1.0, z)

    log_debug(f"Burn tick: {tick_damage:.1f} damage to entity {target_id}")


def get_cooldown_remaining(caster_id: int) -> float:
    """
    Get the remaining cooldown time for Fireball.

    Args:
        caster_id: The caster entity

    Returns:
        Remaining cooldown in seconds, or 0 if ready
    """
    last_cast = get_property(caster_id, f"cooldown_{SPELL_ID}")
    if not last_cast:
        return 0.0

    elapsed = get_time() - last_cast
    remaining = COOLDOWN - elapsed

    return max(0.0, remaining)


def get_spell_info() -> dict:
    """
    Get spell information for UI display.

    Returns:
        Dictionary with spell details
    """
    return {
        "id": SPELL_ID,
        "name": SPELL_NAME,
        "icon": "spell_fireball_icon",
        "description": "Hurls a ball of fire at the target location, dealing damage to all enemies in the area and setting them ablaze.",
        "mana_cost": MANA_COST,
        "cooldown": COOLDOWN,
        "range": RANGE,
        "damage": f"{BASE_DAMAGE} (+{int(SPELL_POWER_SCALING * 100)}% SP)",
        "damage_type": "Fire",
        "effects": [
            f"Deals {BASE_DAMAGE} fire damage",
            f"Area of effect: {AOE_RADIUS}m radius",
            f"Burns enemies for {BURN_DAMAGE_PER_SECOND}/s over {BURN_DURATION}s",
        ],
    }


# Hook into projectile collision system
def on_projectile_hit(projectile_id: int, hit_entity: int = 0) -> None:
    """
    Global projectile hit handler.
    Routes fireball projectiles to the proper impact handler.
    """
    spell_data = get_property(projectile_id, "spell_data")
    if spell_data and spell_data.get("spell_id") == SPELL_ID:
        hit_pos = get_position(projectile_id)
        on_fireball_impact(projectile_id, hit_pos, hit_entity)


# Hook into debuff tick system
def on_debuff_tick(entity_id: int, debuff_name: str, debuff_data: dict) -> None:
    """
    Global debuff tick handler.
    Routes burning ticks to the proper handler.
    """
    if debuff_name == "burning":
        on_burn_tick(entity_id, debuff_data)
