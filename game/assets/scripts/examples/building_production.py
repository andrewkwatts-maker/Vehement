"""
Building Production Example Script
Demonstrates a building that produces units over time with resource costs.

This example shows:
- Production queue management
- Resource cost checking and consumption
- Timed production with progress tracking
- Rally point for produced units
- Production bonuses from technologies
"""

from game_api import *
from typing import List, Dict, Optional, Tuple
from enum import Enum
from dataclasses import dataclass


class ProductionState(Enum):
    """State of a production item."""
    QUEUED = "queued"
    PRODUCING = "producing"
    COMPLETE = "complete"
    CANCELLED = "cancelled"


@dataclass
class ProductionItem:
    """A single item in the production queue."""
    unit_type: str
    progress: float  # 0.0 to 1.0
    state: ProductionState
    queue_time: float
    start_time: float = 0.0


# Configuration
MAX_QUEUE_SIZE = 5
DEFAULT_RALLY_OFFSET = (5.0, 0.0, 0.0)  # Spawn offset from building

# Unit production data (would normally come from data files)
PRODUCIBLE_UNITS = {
    "worker": {
        "name": "Worker",
        "production_time": 15.0,
        "costs": {"gold": 50, "food": 1},
        "tech_requirement": None,
        "icon": "icon_worker",
    },
    "soldier": {
        "name": "Soldier",
        "production_time": 25.0,
        "costs": {"gold": 100, "food": 2},
        "tech_requirement": None,
        "icon": "icon_soldier",
    },
    "archer": {
        "name": "Archer",
        "production_time": 30.0,
        "costs": {"gold": 80, "wood": 40, "food": 2},
        "tech_requirement": "archery",
        "icon": "icon_archer",
    },
    "knight": {
        "name": "Knight",
        "production_time": 45.0,
        "costs": {"gold": 200, "food": 3},
        "tech_requirement": "cavalry",
        "icon": "icon_knight",
    },
}


class BuildingProduction:
    """
    Manages unit production for a building.
    """

    def __init__(self, building_id: int):
        self.building_id = building_id
        self.owner_id = get_owner(building_id)
        self.queue: List[ProductionItem] = []
        self.rally_point: Optional[Tuple[float, float, float]] = None
        self.production_speed_multiplier = 1.0

    def get_producible_units(self) -> List[str]:
        """
        Get list of units this building can currently produce.

        Returns:
            List of unit type IDs that can be produced
        """
        available = []

        for unit_type, data in PRODUCIBLE_UNITS.items():
            # Check technology requirement
            tech_req = data.get("tech_requirement")
            if tech_req and not has_tech(self.owner_id, tech_req):
                continue

            available.append(unit_type)

        return available

    def can_produce(self, unit_type: str) -> Tuple[bool, str]:
        """
        Check if a unit can be added to the production queue.

        Args:
            unit_type: The type of unit to produce

        Returns:
            Tuple of (can_produce, reason_if_not)
        """
        # Check if building is operational
        if not is_alive(self.building_id):
            return False, "Building destroyed"

        building_health = get_health(self.building_id)
        max_health = get_max_health(self.building_id)
        if building_health < max_health * 0.25:
            return False, "Building too damaged"

        # Check if unit type is valid
        if unit_type not in PRODUCIBLE_UNITS:
            return False, f"Unknown unit type: {unit_type}"

        unit_data = PRODUCIBLE_UNITS[unit_type]

        # Check technology requirement
        tech_req = unit_data.get("tech_requirement")
        if tech_req and not has_tech(self.owner_id, tech_req):
            tech_name = get_tech_name(tech_req)
            return False, f"Requires {tech_name}"

        # Check queue size
        if len(self.queue) >= MAX_QUEUE_SIZE:
            return False, "Production queue full"

        # Check resources
        costs = unit_data.get("costs", {})
        for resource, amount in costs.items():
            available = get_resource(self.owner_id, resource)
            if available < amount:
                return False, f"Not enough {resource} ({available}/{amount})"

        # Check population cap
        food_cost = costs.get("food", 0)
        if food_cost > 0:
            current_pop = get_population(self.owner_id)
            max_pop = get_max_population(self.owner_id)
            if current_pop + food_cost > max_pop:
                return False, f"Population cap reached ({current_pop}/{max_pop})"

        return True, ""

    def queue_production(self, unit_type: str) -> bool:
        """
        Add a unit to the production queue.

        Args:
            unit_type: The type of unit to produce

        Returns:
            True if the unit was queued successfully
        """
        can, reason = self.can_produce(unit_type)
        if not can:
            show_notification(self.owner_id, f"Cannot produce: {reason}", "error")
            return False

        unit_data = PRODUCIBLE_UNITS[unit_type]

        # Deduct resources
        costs = unit_data.get("costs", {})
        for resource, amount in costs.items():
            modify_resource(self.owner_id, resource, -amount)

        # Reserve population
        food_cost = costs.get("food", 0)
        if food_cost > 0:
            modify_reserved_population(self.owner_id, food_cost)

        # Add to queue
        item = ProductionItem(
            unit_type=unit_type,
            progress=0.0,
            state=ProductionState.QUEUED,
            queue_time=get_time()
        )
        self.queue.append(item)

        # Start production if this is the first item
        if len(self.queue) == 1:
            self._start_production(item)

        # Play queue sound
        x, y, z = get_position(self.building_id)
        play_sound("production_queue", x, y, z)

        log_debug(f"Building {self.building_id} queued {unit_type}")
        return True

    def cancel_production(self, index: int) -> bool:
        """
        Cancel a queued production item.

        Args:
            index: The queue index to cancel (0 is current production)

        Returns:
            True if cancelled successfully
        """
        if index < 0 or index >= len(self.queue):
            return False

        item = self.queue[index]
        unit_data = PRODUCIBLE_UNITS[item.unit_type]

        # Refund resources (100% for queued, 50% for in-progress)
        costs = unit_data.get("costs", {})
        refund_rate = 0.5 if item.state == ProductionState.PRODUCING else 1.0

        for resource, amount in costs.items():
            refund = int(amount * refund_rate)
            modify_resource(self.owner_id, resource, refund)

        # Release reserved population
        food_cost = costs.get("food", 0)
        if food_cost > 0:
            modify_reserved_population(self.owner_id, -food_cost)

        # Remove from queue
        self.queue.pop(index)
        item.state = ProductionState.CANCELLED

        # Start next item if we cancelled current
        if index == 0 and len(self.queue) > 0:
            self._start_production(self.queue[0])

        log_debug(f"Building {self.building_id} cancelled {item.unit_type} (refund: {int(refund_rate * 100)}%)")
        return True

    def update(self, delta_time: float) -> None:
        """
        Update production progress.

        Args:
            delta_time: Time since last update
        """
        if len(self.queue) == 0:
            return

        # Check if building is operational
        if not is_alive(self.building_id):
            return

        building_health = get_health(self.building_id)
        max_health = get_max_health(self.building_id)
        if building_health < max_health * 0.25:
            return  # Production paused

        current_item = self.queue[0]
        if current_item.state != ProductionState.PRODUCING:
            return

        unit_data = PRODUCIBLE_UNITS[current_item.unit_type]
        production_time = unit_data["production_time"]

        # Apply production speed bonuses
        effective_time = production_time / self._get_production_multiplier()

        # Update progress
        progress_per_second = 1.0 / effective_time
        current_item.progress += progress_per_second * delta_time

        # Check completion
        if current_item.progress >= 1.0:
            self._complete_production(current_item)

    def _start_production(self, item: ProductionItem) -> None:
        """Start producing an item."""
        item.state = ProductionState.PRODUCING
        item.start_time = get_time()

        # Play production start sound
        x, y, z = get_position(self.building_id)
        play_sound("production_start", x, y, z)

        log_debug(f"Building {self.building_id} started producing {item.unit_type}")

    def _complete_production(self, item: ProductionItem) -> None:
        """Complete production and spawn the unit."""
        item.state = ProductionState.COMPLETE
        self.queue.pop(0)

        # Get spawn position
        spawn_pos = self._get_spawn_position()
        x, y, z = spawn_pos

        # Spawn the unit
        unit_id = spawn_entity(item.unit_type, x, y, z)
        set_owner(unit_id, self.owner_id)

        # Release reserved population (unit now counts normally)
        unit_data = PRODUCIBLE_UNITS[item.unit_type]
        food_cost = unit_data.get("costs", {}).get("food", 0)
        if food_cost > 0:
            modify_reserved_population(self.owner_id, -food_cost)

        # Play completion effects
        play_sound("production_complete", x, y, z)
        play_effect("unit_spawn", x, y, z)

        # Send unit to rally point
        if self.rally_point:
            rx, ry, rz = self.rally_point
            command_move(unit_id, rx, ry, rz)

        # Show notification
        unit_name = unit_data["name"]
        show_notification(self.owner_id, f"{unit_name} ready!", "info")

        log_debug(f"Building {self.building_id} produced {item.unit_type} (entity {unit_id})")

        # Start next production
        if len(self.queue) > 0:
            self._start_production(self.queue[0])

    def _get_spawn_position(self) -> Tuple[float, float, float]:
        """Get the position to spawn produced units."""
        bx, by, bz = get_position(self.building_id)

        if self.rally_point:
            # Spawn towards rally point
            rx, ry, rz = self.rally_point
            dx = rx - bx
            dz = rz - bz
            dist = (dx * dx + dz * dz) ** 0.5
            if dist > 0.1:
                # Normalize and scale
                offset_dist = 3.0
                spawn_x = bx + (dx / dist) * offset_dist
                spawn_z = bz + (dz / dist) * offset_dist
                return (spawn_x, by, spawn_z)

        # Default offset
        ox, oy, oz = DEFAULT_RALLY_OFFSET
        return (bx + ox, by + oy, bz + oz)

    def _get_production_multiplier(self) -> float:
        """Get the total production speed multiplier."""
        multiplier = self.production_speed_multiplier

        # Check for technology bonuses
        if has_tech(self.owner_id, "conscription"):
            multiplier *= 1.33  # 33% faster

        if has_tech(self.owner_id, "mass_production"):
            multiplier *= 1.25  # 25% faster

        # Check for building upgrades
        upgrade_level = get_property(self.building_id, "upgrade_level")
        if upgrade_level:
            multiplier *= (1.0 + upgrade_level * 0.1)  # 10% per upgrade level

        return multiplier

    def set_rally_point(self, x: float, y: float, z: float) -> None:
        """Set the rally point for produced units."""
        self.rally_point = (x, y, z)

        # Show rally point marker
        play_effect("rally_point_marker", x, y, z)

        log_debug(f"Building {self.building_id} rally point set to ({x}, {y}, {z})")

    def clear_rally_point(self) -> None:
        """Clear the rally point."""
        self.rally_point = None

    def get_queue_info(self) -> List[Dict]:
        """Get information about the production queue for UI display."""
        info = []

        for i, item in enumerate(self.queue):
            unit_data = PRODUCIBLE_UNITS[item.unit_type]
            info.append({
                "index": i,
                "unit_type": item.unit_type,
                "name": unit_data["name"],
                "icon": unit_data["icon"],
                "progress": item.progress,
                "state": item.state.value,
                "production_time": unit_data["production_time"] / self._get_production_multiplier(),
            })

        return info

    def get_current_progress(self) -> Tuple[str, float]:
        """Get current production item and progress."""
        if len(self.queue) == 0:
            return ("", 0.0)

        item = self.queue[0]
        return (item.unit_type, item.progress)


# Global storage for production controllers
_building_production: Dict[int, BuildingProduction] = {}


def on_create(building_id: int) -> None:
    """
    Called when a production building is created.
    Initialize the production controller.
    """
    _building_production[building_id] = BuildingProduction(building_id)
    log_debug(f"Production controller initialized for building {building_id}")


def on_tick(building_id: int, delta_time: float) -> None:
    """
    Called every frame for production buildings.
    Update production progress.
    """
    if building_id in _building_production:
        _building_production[building_id].update(delta_time)


def on_death(building_id: int, killer_id: int) -> None:
    """
    Called when the building is destroyed.
    Refund queued items and clean up.
    """
    if building_id in _building_production:
        production = _building_production[building_id]

        # Cancel all queued items (refunds resources)
        while len(production.queue) > 0:
            production.cancel_production(0)

        del _building_production[building_id]
        log_debug(f"Production controller cleaned up for building {building_id}")


# External API for game systems
def queue_unit(building_id: int, unit_type: str) -> bool:
    """Queue a unit for production."""
    if building_id in _building_production:
        return _building_production[building_id].queue_production(unit_type)
    return False


def cancel_unit(building_id: int, index: int) -> bool:
    """Cancel a queued unit."""
    if building_id in _building_production:
        return _building_production[building_id].cancel_production(index)
    return False


def set_rally(building_id: int, x: float, y: float, z: float) -> None:
    """Set the rally point."""
    if building_id in _building_production:
        _building_production[building_id].set_rally_point(x, y, z)


def get_queue(building_id: int) -> List[Dict]:
    """Get the production queue."""
    if building_id in _building_production:
        return _building_production[building_id].get_queue_info()
    return []
