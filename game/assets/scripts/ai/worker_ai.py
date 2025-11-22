"""
Worker AI Script for Vehement2 RTS

Implements worker behavior:
- Gather resources from nearby resource nodes
- Return gathered resources to storage buildings
- Rest when tired (low energy)
- Flee from danger (zombies)

This script demonstrates:
- Utility AI scoring functions
- Resource gathering logic
- Need-based decision making
"""

import nova_engine as nova
import math

# ============================================================================
# Constants
# ============================================================================

GATHER_RANGE = 2.0          # Range to gather from resource node
GATHER_TIME = 3.0           # Seconds to gather resources
GATHER_AMOUNT = 10          # Resources gathered per cycle
CARRY_CAPACITY = 50         # Maximum resources a worker can carry
STORAGE_RANGE = 2.0         # Range to deposit at storage
MOVE_SPEED = 3.0            # Normal movement speed
FLEE_SPEED = 5.0            # Movement speed when fleeing
DANGER_RANGE = 15.0         # Range to detect threats
SAFE_DISTANCE = 25.0        # Distance to flee from threats
REST_THRESHOLD = 20.0       # Energy level to trigger rest
REST_RATE = 15.0            # Energy recovered per second when resting
WORK_ENERGY_COST = 5.0      # Energy consumed per second when working
MAX_ENERGY = 100.0          # Maximum energy level

# ============================================================================
# Worker States
# ============================================================================

class WorkerState:
    IDLE = "idle"
    MOVING_TO_RESOURCE = "moving_to_resource"
    GATHERING = "gathering"
    MOVING_TO_STORAGE = "moving_to_storage"
    DEPOSITING = "depositing"
    RESTING = "resting"
    FLEEING = "fleeing"

# Per-entity state storage
worker_states = {}

def get_worker_state(entity_id):
    """Get or create state for a worker entity."""
    if entity_id not in worker_states:
        worker_states[entity_id] = {
            "state": WorkerState.IDLE,
            "energy": MAX_ENERGY,
            "carried_resource": "",
            "carried_amount": 0,
            "target_position": None,
            "target_building": 0,
            "target_resource_node": 0,
            "gather_timer": 0.0,
            "rest_timer": 0.0,
            "home_building": 0,
            "assigned_resource": "Wood",  # Default resource to gather
        }
    return worker_states[entity_id]

# ============================================================================
# Main Update Function
# ============================================================================

def update(entity_id, delta_time):
    """
    Main update function called by the engine each frame.
    """
    state = get_worker_state(entity_id)
    entity = nova.get_entity(entity_id)

    if not entity.is_alive:
        cleanup(entity_id)
        return "failure"

    # Update energy consumption/recovery
    update_energy(entity_id, delta_time, state)

    # Check for danger first (highest priority)
    if should_flee(entity_id, state):
        state["state"] = WorkerState.FLEEING

    # State machine update
    current_state = state["state"]

    if current_state == WorkerState.FLEEING:
        update_fleeing(entity_id, delta_time, state)
    elif current_state == WorkerState.RESTING:
        update_resting(entity_id, delta_time, state)
    elif current_state == WorkerState.GATHERING:
        update_gathering(entity_id, delta_time, state)
    elif current_state == WorkerState.DEPOSITING:
        update_depositing(entity_id, delta_time, state)
    elif current_state == WorkerState.MOVING_TO_RESOURCE:
        update_moving_to_resource(entity_id, delta_time, state)
    elif current_state == WorkerState.MOVING_TO_STORAGE:
        update_moving_to_storage(entity_id, delta_time, state)
    else:  # IDLE
        update_idle(entity_id, delta_time, state)

    return "running"

# ============================================================================
# State Update Functions
# ============================================================================

def update_idle(entity_id, delta_time, state):
    """Idle state - decide what to do next."""
    # Check if we need rest
    if state["energy"] < REST_THRESHOLD:
        state["state"] = WorkerState.RESTING
        return

    # If carrying resources, go deposit them
    if state["carried_amount"] > 0:
        storage_pos = find_nearest_storage(entity_id)
        if storage_pos is not None:
            state["target_position"] = storage_pos
            state["state"] = WorkerState.MOVING_TO_STORAGE
            return

    # Find a resource to gather
    resource_pos = find_nearest_resource(entity_id, state["assigned_resource"])
    if resource_pos is not None:
        state["target_position"] = resource_pos
        state["state"] = WorkerState.MOVING_TO_RESOURCE
        return

    # Nothing to do, just wait
    pass

def update_moving_to_resource(entity_id, delta_time, state):
    """Moving to resource node state."""
    entity = nova.get_entity(entity_id)
    target = state["target_position"]

    if target is None:
        state["state"] = WorkerState.IDLE
        return

    distance = entity.distance_to_point(target)

    if distance <= GATHER_RANGE:
        # Arrived at resource, start gathering
        state["state"] = WorkerState.GATHERING
        state["gather_timer"] = GATHER_TIME
        return

    # Move toward target
    move_toward(entity_id, target, MOVE_SPEED * delta_time)

def update_gathering(entity_id, delta_time, state):
    """Gathering resources state."""
    entity = nova.get_entity(entity_id)

    # Consume energy while working
    state["energy"] -= WORK_ENERGY_COST * delta_time

    # Progress gathering
    state["gather_timer"] -= delta_time

    if state["gather_timer"] <= 0:
        # Finished gathering
        state["carried_resource"] = state["assigned_resource"]
        state["carried_amount"] = min(
            state["carried_amount"] + GATHER_AMOUNT,
            CARRY_CAPACITY
        )

        nova.log_info(f"Worker {entity_id} gathered {GATHER_AMOUNT} {state['assigned_resource']}")

        # Decide: keep gathering or return to storage
        if state["carried_amount"] >= CARRY_CAPACITY:
            storage_pos = find_nearest_storage(entity_id)
            if storage_pos is not None:
                state["target_position"] = storage_pos
                state["state"] = WorkerState.MOVING_TO_STORAGE
            else:
                state["state"] = WorkerState.IDLE
        else:
            # Continue gathering
            state["gather_timer"] = GATHER_TIME

def update_moving_to_storage(entity_id, delta_time, state):
    """Moving to storage building state."""
    entity = nova.get_entity(entity_id)
    target = state["target_position"]

    if target is None:
        state["state"] = WorkerState.IDLE
        return

    distance = entity.distance_to_point(target)

    if distance <= STORAGE_RANGE:
        # Arrived at storage, deposit
        state["state"] = WorkerState.DEPOSITING
        return

    # Move toward target
    move_toward(entity_id, target, MOVE_SPEED * delta_time)

def update_depositing(entity_id, delta_time, state):
    """Depositing resources state."""
    # Add resources to stockpile
    if state["carried_amount"] > 0:
        nova.add_resource(state["carried_resource"], state["carried_amount"])
        nova.show_notification(
            f"+{state['carried_amount']} {state['carried_resource']}",
            1.5
        )
        state["carried_amount"] = 0
        state["carried_resource"] = ""

    # Go back to idle to decide next action
    state["state"] = WorkerState.IDLE

def update_resting(entity_id, delta_time, state):
    """Resting state - recover energy."""
    state["energy"] = min(state["energy"] + REST_RATE * delta_time, MAX_ENERGY)

    if state["energy"] >= MAX_ENERGY * 0.8:
        # Recovered enough, back to work
        state["state"] = WorkerState.IDLE
        nova.log_info(f"Worker {entity_id} finished resting")

def update_fleeing(entity_id, delta_time, state):
    """Fleeing from danger state."""
    entity = nova.get_entity(entity_id)

    # Find direction away from nearest threat
    threat_id = find_nearest_threat(entity_id)

    if threat_id == 0:
        # No more threats, return to previous activity
        state["state"] = WorkerState.IDLE
        return

    threat = nova.get_entity(threat_id)
    distance = entity.distance_to(threat)

    if distance > SAFE_DISTANCE:
        # Safe distance reached
        state["state"] = WorkerState.IDLE
        return

    # Move away from threat
    direction = nova.Vector3(
        entity.position.x - threat.position.x,
        0,
        entity.position.z - threat.position.z
    ).normalized()

    new_pos = nova.Vector3(
        entity.position.x + direction.x * FLEE_SPEED * delta_time,
        entity.position.y,
        entity.position.z + direction.z * FLEE_SPEED * delta_time
    )
    entity.position = new_pos

# ============================================================================
# Helper Functions
# ============================================================================

def update_energy(entity_id, delta_time, state):
    """Update worker energy based on activity."""
    if state["state"] == WorkerState.RESTING:
        return  # Energy recovery handled in update_resting

    # Base energy drain
    base_drain = 0.5
    state["energy"] = max(0, state["energy"] - base_drain * delta_time)

def should_flee(entity_id, state):
    """Check if worker should flee from danger."""
    if state["state"] == WorkerState.FLEEING:
        return True  # Already fleeing

    threat_id = find_nearest_threat(entity_id)
    return threat_id > 0

def find_nearest_threat(entity_id):
    """Find nearest zombie or hostile entity."""
    entity = nova.get_entity(entity_id)
    return nova.get_nearest_entity(
        entity.position.x, entity.position.y, entity.position.z, "Zombie"
    )

def find_nearest_resource(entity_id, resource_type):
    """Find nearest resource node of the given type."""
    # For now, return a position offset from the entity
    # In a real implementation, this would query resource nodes
    entity = nova.get_entity(entity_id)

    # Simulate finding a resource node nearby
    angle = nova.random_range(0, 2 * 3.14159)
    distance = nova.random_range(5.0, 15.0)

    return nova.Vector3(
        entity.position.x + math.cos(angle) * distance,
        entity.position.y,
        entity.position.z + math.sin(angle) * distance
    )

def find_nearest_storage(entity_id):
    """Find nearest storage building."""
    entity = nova.get_entity(entity_id)
    state = get_worker_state(entity_id)

    # If we have a home building, go there
    if state["home_building"] > 0:
        building_pos = nova.get_entity(state["home_building"])
        if building_pos is not None:
            return building_pos.position

    # Otherwise, return to origin area
    return nova.Vector3(0, entity.position.y, 0)

def move_toward(entity_id, target, move_amount):
    """Move entity toward a target position."""
    entity = nova.get_entity(entity_id)

    direction = nova.Vector3(
        target.x - entity.position.x,
        0,
        target.z - entity.position.z
    )

    distance = direction.length()
    if distance < 0.1:
        return

    direction = direction.normalized()

    move_dist = min(move_amount, distance)
    new_pos = nova.Vector3(
        entity.position.x + direction.x * move_dist,
        entity.position.y,
        entity.position.z + direction.z * move_dist
    )
    entity.position = new_pos

# ============================================================================
# Utility AI Scoring Functions
# ============================================================================

def score_gather(entity_id):
    """Score for gathering resources."""
    state = get_worker_state(entity_id)

    # Lower score if carrying near capacity
    carry_ratio = state["carried_amount"] / CARRY_CAPACITY

    # Lower score if low energy
    energy_ratio = state["energy"] / MAX_ENERGY

    # Base score
    score = 0.5

    # Modify based on carry capacity
    score *= (1.0 - carry_ratio * 0.5)

    # Modify based on energy
    if energy_ratio < 0.3:
        score *= 0.3

    return score

def score_deposit(entity_id):
    """Score for depositing resources."""
    state = get_worker_state(entity_id)

    if state["carried_amount"] == 0:
        return 0.0

    carry_ratio = state["carried_amount"] / CARRY_CAPACITY

    # Higher score if carrying more
    return 0.3 + carry_ratio * 0.7

def score_rest(entity_id):
    """Score for resting."""
    state = get_worker_state(entity_id)
    energy_ratio = state["energy"] / MAX_ENERGY

    if energy_ratio > 0.5:
        return 0.0

    # Higher score when more tired
    return 1.0 - energy_ratio

def score_flee(entity_id):
    """Score for fleeing from danger."""
    threat_id = find_nearest_threat(entity_id)

    if threat_id == 0:
        return 0.0

    entity = nova.get_entity(entity_id)
    threat = nova.get_entity(threat_id)
    distance = entity.distance_to(threat)

    if distance > DANGER_RANGE:
        return 0.0

    # Higher score when threat is closer
    return 1.0 - (distance / DANGER_RANGE)

# ============================================================================
# Event Handlers
# ============================================================================

def on_damage(entity_id, damage_amount, source_id):
    """Called when worker takes damage."""
    state = get_worker_state(entity_id)

    # Immediately flee when damaged
    state["state"] = WorkerState.FLEEING
    nova.show_warning(f"Worker under attack!")

def on_assigned_to_building(entity_id, building_id, job_type):
    """Called when worker is assigned to a building."""
    state = get_worker_state(entity_id)

    if job_type == "gather":
        state["home_building"] = building_id
        state["state"] = WorkerState.IDLE
        nova.log_info(f"Worker {entity_id} assigned to building {building_id}")

def on_resource_type_changed(entity_id, resource_type):
    """Called when worker's assigned resource type changes."""
    state = get_worker_state(entity_id)
    state["assigned_resource"] = resource_type
    nova.log_info(f"Worker {entity_id} now gathering {resource_type}")

# ============================================================================
# Lifecycle Functions
# ============================================================================

def init(entity_id):
    """Called when the worker is spawned."""
    nova.log_info(f"Worker {entity_id} initialized")
    get_worker_state(entity_id)
    return "success"

def cleanup(entity_id):
    """Called when the worker is destroyed."""
    if entity_id in worker_states:
        del worker_states[entity_id]
    return "success"
