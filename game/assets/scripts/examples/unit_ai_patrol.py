"""
Unit AI Patrol Example Script
Demonstrates a patrolling unit that detects and engages enemies.

This example shows:
- State machine AI
- Patrol point navigation
- Enemy detection and targeting
- Combat engagement
- Return to patrol behavior
"""

from game_api import *
from enum import Enum
from typing import List, Tuple, Optional
import math


class AIState(Enum):
    """AI behavior states."""
    IDLE = "idle"
    PATROL = "patrol"
    ALERT = "alert"
    CHASE = "chase"
    ATTACK = "attack"
    RETURN = "return"


# Configuration
PATROL_SPEED = 3.0
CHASE_SPEED = 5.0
DETECTION_RADIUS = 15.0
ATTACK_RANGE = 2.0
LEASH_RADIUS = 30.0  # Max distance from home before returning
ATTACK_COOLDOWN = 1.5
ALERT_DURATION = 2.0  # Time to wait before chasing


class PatrolAI:
    """
    A patrol AI controller that handles idle, patrol, chase, and attack behaviors.
    """

    def __init__(self, entity_id: int):
        self.entity_id = entity_id
        self.state = AIState.IDLE
        self.home_position = get_position(entity_id)
        self.patrol_points: List[Tuple[float, float, float]] = []
        self.current_patrol_index = 0
        self.target_id = 0
        self.last_attack_time = 0.0
        self.alert_start_time = 0.0
        self.idle_time = 0.0
        self.idle_duration = random_range(2.0, 5.0)

    def setup_patrol_route(self, points: List[Tuple[float, float, float]]) -> None:
        """Set up the patrol route with waypoints."""
        self.patrol_points = points
        self.current_patrol_index = 0
        log_debug(f"Patrol route set with {len(points)} waypoints")

    def setup_patrol_radius(self, radius: float, num_points: int = 4) -> None:
        """Generate a circular patrol route around home position."""
        hx, hy, hz = self.home_position
        self.patrol_points = []

        for i in range(num_points):
            angle = (2 * math.pi * i) / num_points
            px = hx + radius * math.cos(angle)
            pz = hz + radius * math.sin(angle)
            self.patrol_points.append((px, hy, pz))

        log_debug(f"Generated circular patrol with {num_points} points, radius {radius}")

    def update(self, delta_time: float) -> None:
        """Main AI update - called every frame."""
        current_time = get_time()
        x, y, z = get_position(self.entity_id)

        # Always check for enemies (unless already in combat)
        if self.state not in [AIState.CHASE, AIState.ATTACK]:
            enemy = self._find_nearest_enemy(x, y, z)
            if enemy:
                self._on_enemy_detected(enemy)

        # State machine update
        if self.state == AIState.IDLE:
            self._update_idle(delta_time, x, y, z)
        elif self.state == AIState.PATROL:
            self._update_patrol(delta_time, x, y, z)
        elif self.state == AIState.ALERT:
            self._update_alert(delta_time, x, y, z, current_time)
        elif self.state == AIState.CHASE:
            self._update_chase(delta_time, x, y, z)
        elif self.state == AIState.ATTACK:
            self._update_attack(delta_time, x, y, z, current_time)
        elif self.state == AIState.RETURN:
            self._update_return(delta_time, x, y, z)

    def _update_idle(self, delta_time: float, x: float, y: float, z: float) -> None:
        """Handle idle state - wait, then start patrolling."""
        self.idle_time += delta_time

        # Play idle animation
        play_animation(self.entity_id, "idle")

        # Occasionally look around
        if random_chance(0.01):
            random_yaw = random_range(0, 360)
            set_rotation(self.entity_id, 0, random_yaw, 0)

        # Transition to patrol after idle duration
        if self.idle_time >= self.idle_duration:
            if len(self.patrol_points) > 0:
                self.state = AIState.PATROL
                log_debug(f"Entity {self.entity_id} starting patrol")
            self.idle_time = 0
            self.idle_duration = random_range(2.0, 5.0)

    def _update_patrol(self, delta_time: float, x: float, y: float, z: float) -> None:
        """Handle patrol state - move between waypoints."""
        if len(self.patrol_points) == 0:
            self.state = AIState.IDLE
            return

        # Get current target waypoint
        target = self.patrol_points[self.current_patrol_index]
        tx, ty, tz = target

        # Calculate distance to waypoint
        dist = distance_3d(x, y, z, tx, ty, tz)

        if dist < 1.5:
            # Reached waypoint - move to next
            self.current_patrol_index = (self.current_patrol_index + 1) % len(self.patrol_points)

            # Brief pause at waypoint
            self.state = AIState.IDLE
            self.idle_duration = random_range(1.0, 3.0)
            log_debug(f"Entity {self.entity_id} reached waypoint, next: {self.current_patrol_index}")
        else:
            # Move towards waypoint
            move_towards(self.entity_id, tx, ty, tz, PATROL_SPEED)
            play_animation(self.entity_id, "walk")

    def _update_alert(self, delta_time: float, x: float, y: float, z: float, current_time: float) -> None:
        """Handle alert state - spotted enemy, preparing to engage."""
        # Face the enemy
        if self.target_id > 0 and is_alive(self.target_id):
            tx, ty, tz = get_position(self.target_id)
            look_at(self.entity_id, tx, ty, tz)

        # Play alert animation
        play_animation(self.entity_id, "alert")

        # Check if alert duration has passed
        if current_time - self.alert_start_time >= ALERT_DURATION:
            if self.target_id > 0 and is_alive(self.target_id):
                self.state = AIState.CHASE
                log_debug(f"Entity {self.entity_id} engaging target {self.target_id}")
            else:
                self.target_id = 0
                self.state = AIState.PATROL

    def _update_chase(self, delta_time: float, x: float, y: float, z: float) -> None:
        """Handle chase state - pursue enemy."""
        # Check if target is still valid
        if self.target_id == 0 or not is_alive(self.target_id):
            self.target_id = 0
            self.state = AIState.RETURN
            return

        # Get target position
        tx, ty, tz = get_position(self.target_id)

        # Check leash distance from home
        home_dist = distance_3d(x, y, z, *self.home_position)
        if home_dist > LEASH_RADIUS:
            log_debug(f"Entity {self.entity_id} exceeded leash distance, returning home")
            self.target_id = 0
            self.state = AIState.RETURN
            return

        # Calculate distance to target
        target_dist = distance_3d(x, y, z, tx, ty, tz)

        if target_dist <= ATTACK_RANGE:
            # In attack range - start attacking
            self.state = AIState.ATTACK
        else:
            # Chase target
            move_towards(self.entity_id, tx, ty, tz, CHASE_SPEED)
            play_animation(self.entity_id, "run")

    def _update_attack(self, delta_time: float, x: float, y: float, z: float, current_time: float) -> None:
        """Handle attack state - attack the target."""
        # Check if target is still valid
        if self.target_id == 0 or not is_alive(self.target_id):
            self.target_id = 0
            self.state = AIState.RETURN
            return

        # Get target position
        tx, ty, tz = get_position(self.target_id)
        target_dist = distance_3d(x, y, z, tx, ty, tz)

        # Face target
        look_at(self.entity_id, tx, ty, tz)

        # Check if target moved out of range
        if target_dist > ATTACK_RANGE * 1.5:
            self.state = AIState.CHASE
            return

        # Attack if cooldown is ready
        if current_time - self.last_attack_time >= ATTACK_COOLDOWN:
            self._perform_attack()
            self.last_attack_time = current_time

    def _update_return(self, delta_time: float, x: float, y: float, z: float) -> None:
        """Handle return state - go back to home position."""
        hx, hy, hz = self.home_position
        dist = distance_3d(x, y, z, hx, hy, hz)

        if dist < 2.0:
            # Reached home - resume patrol or idle
            self.state = AIState.IDLE
            log_debug(f"Entity {self.entity_id} returned home")
        else:
            # Move towards home
            move_towards(self.entity_id, hx, hy, hz, PATROL_SPEED)
            play_animation(self.entity_id, "walk")

    def _find_nearest_enemy(self, x: float, y: float, z: float) -> Optional[int]:
        """Find the nearest enemy within detection radius."""
        nearby = find_entities_in_radius(x, y, z, DETECTION_RADIUS)

        # Filter to enemies that are alive
        enemies = []
        for entity in nearby:
            if is_enemy(self.entity_id, entity) and is_alive(entity):
                enemies.append(entity)

        if not enemies:
            return None

        # Return nearest
        return min(enemies, key=lambda e: distance_to(self.entity_id, e))

    def _on_enemy_detected(self, enemy_id: int) -> None:
        """Called when an enemy is first detected."""
        self.target_id = enemy_id
        self.state = AIState.ALERT
        self.alert_start_time = get_time()

        # Play alert sound and effect
        x, y, z = get_position(self.entity_id)
        play_sound("alert_sound", x, y, z)
        play_effect("exclamation_mark", x, y + 2.5, z)

        log_debug(f"Entity {self.entity_id} detected enemy {enemy_id}")

    def _perform_attack(self) -> None:
        """Execute an attack on the current target."""
        if self.target_id == 0:
            return

        x, y, z = get_position(self.entity_id)

        # Get attack damage from entity properties
        attack_damage = get_property(self.entity_id, "attack_damage")
        if attack_damage is None:
            attack_damage = 10.0

        # Deal damage
        damage(self.target_id, attack_damage, "physical", self.entity_id)

        # Play attack animation and effects
        play_animation(self.entity_id, "attack")
        play_sound("attack_hit", x, y, z)

        log_debug(f"Entity {self.entity_id} attacked {self.target_id} for {attack_damage} damage")


# Global storage for AI instances
_patrol_ais: dict = {}


def on_create(entity_id: int) -> None:
    """
    Called when the entity is created.
    Initialize the patrol AI.
    """
    ai = PatrolAI(entity_id)

    # Set up a default patrol route around spawn point
    ai.setup_patrol_radius(radius=10.0, num_points=4)

    _patrol_ais[entity_id] = ai
    log_debug(f"Patrol AI initialized for entity {entity_id}")


def on_tick(entity_id: int, delta_time: float) -> None:
    """
    Called every frame.
    Update the AI behavior.
    """
    if entity_id in _patrol_ais:
        _patrol_ais[entity_id].update(delta_time)


def on_death(entity_id: int, killer_id: int) -> None:
    """
    Called when the entity dies.
    Clean up the AI instance.
    """
    if entity_id in _patrol_ais:
        del _patrol_ais[entity_id]
        log_debug(f"Patrol AI cleaned up for entity {entity_id}")


def set_patrol_points(entity_id: int, points: List[Tuple[float, float, float]]) -> None:
    """
    External function to set custom patrol points for an entity.

    Args:
        entity_id: The entity to configure
        points: List of (x, y, z) waypoints
    """
    if entity_id in _patrol_ais:
        _patrol_ais[entity_id].setup_patrol_route(points)
