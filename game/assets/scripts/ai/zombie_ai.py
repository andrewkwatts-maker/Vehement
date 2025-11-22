"""
Zombie AI Script for Vehement2

Implements basic zombie behavior:
- Chase nearest player when in range
- Wander randomly when idle (no target)
- Attack when close enough
- Group behavior with other zombies

This script demonstrates:
- State machine pattern in Python
- Using nova_engine bindings
- Behavior tree node implementation
"""

import nova_engine as nova
import math
import random

# ============================================================================
# Constants
# ============================================================================

DETECTION_RANGE = 20.0      # Range to detect player
ATTACK_RANGE = 1.5          # Range to attack
WANDER_RANGE = 10.0         # How far to wander
WANDER_INTERVAL = 3.0       # Seconds between wander decisions
ATTACK_COOLDOWN = 1.0       # Seconds between attacks
CHASE_SPEED = 4.0           # Movement speed when chasing
WANDER_SPEED = 2.0          # Movement speed when wandering
ATTACK_DAMAGE = 10.0        # Damage per attack
GROUP_RADIUS = 8.0          # Radius to find other zombies for group behavior

# ============================================================================
# State Machine States
# ============================================================================

class ZombieState:
    IDLE = "idle"
    WANDER = "wander"
    CHASE = "chase"
    ATTACK = "attack"
    STUNNED = "stunned"

# Per-entity state storage
zombie_states = {}

def get_zombie_state(entity_id):
    """Get or create state for a zombie entity."""
    if entity_id not in zombie_states:
        zombie_states[entity_id] = {
            "state": ZombieState.IDLE,
            "target_id": 0,
            "wander_target": None,
            "wander_timer": 0.0,
            "attack_timer": 0.0,
            "stun_timer": 0.0,
            "home_position": None,
        }
    return zombie_states[entity_id]

# ============================================================================
# Main Update Function (called each frame)
# ============================================================================

def update(entity_id, delta_time):
    """
    Main update function called by the engine each frame.

    Args:
        entity_id: The zombie's entity ID
        delta_time: Time since last frame in seconds

    Returns:
        str: Behavior status ("running", "success", "failure")
    """
    state = get_zombie_state(entity_id)
    entity = nova.get_entity(entity_id)

    if not entity.is_alive:
        cleanup(entity_id)
        return "failure"

    # Update timers
    state["wander_timer"] -= delta_time
    state["attack_timer"] -= delta_time

    # Store home position on first update
    if state["home_position"] is None:
        state["home_position"] = entity.position

    # State machine update
    current_state = state["state"]

    if current_state == ZombieState.STUNNED:
        update_stunned(entity_id, delta_time, state)
    elif current_state == ZombieState.ATTACK:
        update_attack(entity_id, delta_time, state)
    elif current_state == ZombieState.CHASE:
        update_chase(entity_id, delta_time, state)
    elif current_state == ZombieState.WANDER:
        update_wander(entity_id, delta_time, state)
    else:  # IDLE
        update_idle(entity_id, delta_time, state)

    return "running"

# ============================================================================
# State Update Functions
# ============================================================================

def update_idle(entity_id, delta_time, state):
    """Idle state - look for targets, start wandering if bored."""
    entity = nova.get_entity(entity_id)

    # Look for player
    target_id = find_target(entity_id)
    if target_id > 0:
        state["target_id"] = target_id
        state["state"] = ZombieState.CHASE
        nova.log_info(f"Zombie {entity_id} spotted target {target_id}")
        return

    # Start wandering after a delay
    if state["wander_timer"] <= 0:
        state["wander_target"] = pick_wander_target(entity_id)
        state["state"] = ZombieState.WANDER
        state["wander_timer"] = WANDER_INTERVAL + nova.random_range(-1.0, 1.0)

def update_wander(entity_id, delta_time, state):
    """Wander state - move to random nearby location."""
    entity = nova.get_entity(entity_id)

    # Check for targets while wandering
    target_id = find_target(entity_id)
    if target_id > 0:
        state["target_id"] = target_id
        state["state"] = ZombieState.CHASE
        return

    # Move toward wander target
    if state["wander_target"] is not None:
        target = state["wander_target"]
        direction = nova.Vector3(
            target.x - entity.position.x,
            0,
            target.z - entity.position.z
        )

        distance = direction.length()

        if distance < 0.5:
            # Reached wander target, go back to idle
            state["state"] = ZombieState.IDLE
            state["wander_target"] = None
            return

        # Move toward target
        direction = direction.normalized()
        move_amount = WANDER_SPEED * delta_time

        new_pos = nova.Vector3(
            entity.position.x + direction.x * move_amount,
            entity.position.y,
            entity.position.z + direction.z * move_amount
        )
        entity.position = new_pos

def update_chase(entity_id, delta_time, state):
    """Chase state - pursue the target."""
    entity = nova.get_entity(entity_id)
    target_id = state["target_id"]

    # Check if target is still valid
    if target_id <= 0:
        state["state"] = ZombieState.IDLE
        state["target_id"] = 0
        return

    target = nova.get_entity(target_id)
    if not target.is_alive:
        state["state"] = ZombieState.IDLE
        state["target_id"] = 0
        return

    # Check distance to target
    distance = entity.distance_to(target)

    if distance > DETECTION_RANGE * 1.5:
        # Lost target, go back to idle
        state["state"] = ZombieState.IDLE
        state["target_id"] = 0
        nova.log_info(f"Zombie {entity_id} lost target")
        return

    if distance <= ATTACK_RANGE:
        # Close enough to attack
        state["state"] = ZombieState.ATTACK
        return

    # Move toward target
    direction = nova.Vector3(
        target.position.x - entity.position.x,
        0,
        target.position.z - entity.position.z
    )
    direction = direction.normalized()

    # Apply group behavior - slight attraction to nearby zombies
    group_offset = calculate_group_offset(entity_id)
    direction = nova.Vector3(
        direction.x + group_offset.x * 0.2,
        0,
        direction.z + group_offset.z * 0.2
    ).normalized()

    move_amount = CHASE_SPEED * delta_time
    new_pos = nova.Vector3(
        entity.position.x + direction.x * move_amount,
        entity.position.y,
        entity.position.z + direction.z * move_amount
    )
    entity.position = new_pos

def update_attack(entity_id, delta_time, state):
    """Attack state - damage the target if in range."""
    entity = nova.get_entity(entity_id)
    target_id = state["target_id"]

    if target_id <= 0:
        state["state"] = ZombieState.IDLE
        return

    target = nova.get_entity(target_id)
    if not target.is_alive:
        state["state"] = ZombieState.IDLE
        state["target_id"] = 0
        nova.show_notification("Zombie's target died!", 2.0)
        return

    distance = entity.distance_to(target)

    if distance > ATTACK_RANGE:
        # Target moved away, chase again
        state["state"] = ZombieState.CHASE
        return

    # Perform attack if cooldown is ready
    if state["attack_timer"] <= 0:
        target.damage(ATTACK_DAMAGE, entity_id)
        state["attack_timer"] = ATTACK_COOLDOWN

        # Play attack sound and effect
        nova.play_sound("zombie_attack", entity.position.x, entity.position.y, entity.position.z)
        nova.spawn_effect("blood_splatter", target.position.x, target.position.y, target.position.z)

def update_stunned(entity_id, delta_time, state):
    """Stunned state - cannot act for a duration."""
    state["stun_timer"] -= delta_time

    if state["stun_timer"] <= 0:
        state["state"] = ZombieState.IDLE
        nova.log_info(f"Zombie {entity_id} recovered from stun")

# ============================================================================
# Helper Functions
# ============================================================================

def find_target(entity_id):
    """Find the nearest valid target (player) within detection range."""
    entity = nova.get_entity(entity_id)

    # Get nearest player
    target_id = nova.get_nearest_entity(
        entity.position.x, entity.position.y, entity.position.z, "Player"
    )

    if target_id > 0:
        target = nova.get_entity(target_id)
        if target.is_alive and entity.distance_to(target) <= DETECTION_RANGE:
            return target_id

    return 0

def pick_wander_target(entity_id):
    """Pick a random nearby location to wander to."""
    entity = nova.get_entity(entity_id)
    state = get_zombie_state(entity_id)

    home = state["home_position"]
    if home is None:
        home = entity.position

    # Random offset from home position
    angle = nova.random_range(0, 2 * 3.14159)
    distance = nova.random_range(2.0, WANDER_RANGE)

    return nova.Vector3(
        home.x + math.cos(angle) * distance,
        home.y,
        home.z + math.sin(angle) * distance
    )

def calculate_group_offset(entity_id):
    """Calculate offset to move toward nearby zombie group."""
    entity = nova.get_entity(entity_id)

    # Find nearby zombies
    nearby = nova.find_entities_in_radius_vec(entity.position, GROUP_RADIUS)

    if len(nearby) <= 1:
        return nova.Vector3.zero()

    # Calculate average position of nearby zombies (excluding self)
    avg_x, avg_z = 0.0, 0.0
    count = 0

    for other_id in nearby:
        if other_id != entity_id:
            other = nova.get_entity(other_id)
            if other.type == "Zombie":
                avg_x += other.position.x
                avg_z += other.position.z
                count += 1

    if count == 0:
        return nova.Vector3.zero()

    avg_x /= count
    avg_z /= count

    # Direction toward group center
    return nova.Vector3(
        avg_x - entity.position.x,
        0,
        avg_z - entity.position.z
    ).normalized()

# ============================================================================
# Event Handlers
# ============================================================================

def on_damage(entity_id, damage_amount, source_id):
    """Called when zombie takes damage."""
    state = get_zombie_state(entity_id)

    # If we don't have a target, target the attacker
    if state["target_id"] == 0 and source_id > 0:
        state["target_id"] = source_id
        state["state"] = ZombieState.CHASE
        nova.log_info(f"Zombie {entity_id} targeting attacker {source_id}")

    # Chance to alert nearby zombies
    if nova.random() < 0.3:
        alert_nearby_zombies(entity_id, source_id)

def on_stun(entity_id, duration):
    """Called when zombie is stunned (e.g., by flashbang)."""
    state = get_zombie_state(entity_id)
    state["state"] = ZombieState.STUNNED
    state["stun_timer"] = duration
    nova.log_info(f"Zombie {entity_id} stunned for {duration}s")

def alert_nearby_zombies(entity_id, target_id):
    """Alert nearby zombies to a target."""
    entity = nova.get_entity(entity_id)
    nearby = nova.find_entities_in_radius_vec(entity.position, DETECTION_RANGE)

    for other_id in nearby:
        if other_id != entity_id:
            other = nova.get_entity(other_id)
            if other.type == "Zombie" and other.is_alive:
                other_state = get_zombie_state(other_id)
                if other_state["target_id"] == 0:
                    other_state["target_id"] = target_id
                    other_state["state"] = ZombieState.CHASE

# ============================================================================
# Lifecycle Functions
# ============================================================================

def init(entity_id):
    """Called when the zombie is spawned."""
    nova.log_info(f"Zombie {entity_id} initialized")
    get_zombie_state(entity_id)  # Initialize state
    return "success"

def cleanup(entity_id):
    """Called when the zombie is destroyed."""
    if entity_id in zombie_states:
        del zombie_states[entity_id]
    return "success"

# ============================================================================
# Behavior Tree Node Functions
# ============================================================================

def bt_has_target(entity_id, delta_time):
    """Behavior tree condition: check if zombie has a target."""
    state = get_zombie_state(entity_id)
    return "success" if state["target_id"] > 0 else "failure"

def bt_target_in_attack_range(entity_id, delta_time):
    """Behavior tree condition: check if target is in attack range."""
    state = get_zombie_state(entity_id)
    if state["target_id"] <= 0:
        return "failure"

    entity = nova.get_entity(entity_id)
    target = nova.get_entity(state["target_id"])

    if not target.is_alive:
        return "failure"

    return "success" if entity.distance_to(target) <= ATTACK_RANGE else "failure"

def bt_find_target(entity_id, delta_time):
    """Behavior tree action: find a target."""
    target_id = find_target(entity_id)
    if target_id > 0:
        state = get_zombie_state(entity_id)
        state["target_id"] = target_id
        return "success"
    return "failure"

def bt_chase_target(entity_id, delta_time):
    """Behavior tree action: chase the current target."""
    state = get_zombie_state(entity_id)
    state["state"] = ZombieState.CHASE
    update_chase(entity_id, delta_time, state)
    return "running"

def bt_attack_target(entity_id, delta_time):
    """Behavior tree action: attack the current target."""
    state = get_zombie_state(entity_id)
    state["state"] = ZombieState.ATTACK
    update_attack(entity_id, delta_time, state)
    return "running"

def bt_wander(entity_id, delta_time):
    """Behavior tree action: wander randomly."""
    state = get_zombie_state(entity_id)
    if state["wander_target"] is None:
        state["wander_target"] = pick_wander_target(entity_id)
    state["state"] = ZombieState.WANDER
    update_wander(entity_id, delta_time, state)
    return "running"
