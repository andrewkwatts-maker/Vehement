"""
Hero Level Up Example Script
Demonstrates a hero leveling system with experience, stats, and abilities.

This example shows:
- Experience gain and level progression
- Stat scaling with level
- Ability unlocking at level thresholds
- Skill point allocation
- Level-up visual and audio effects
"""

from game_api import *
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass, field
from enum import Enum


# Level Configuration
MAX_LEVEL = 20
BASE_XP_TO_LEVEL = 100  # XP needed for level 2
XP_SCALING = 1.5  # Each level requires 50% more XP than previous


# Base stats at level 1
BASE_STATS = {
    "max_health": 100,
    "max_mana": 50,
    "attack_damage": 15,
    "armor": 5,
    "magic_resist": 5,
    "attack_speed": 1.0,
    "move_speed": 5.0,
    "health_regen": 1.0,
    "mana_regen": 0.5,
}

# Stat gain per level
STAT_GROWTH = {
    "max_health": 25,
    "max_mana": 15,
    "attack_damage": 3,
    "armor": 1,
    "magic_resist": 1,
    "attack_speed": 0.02,
    "move_speed": 0.1,
    "health_regen": 0.3,
    "mana_regen": 0.15,
}


class AbilityType(Enum):
    """Types of hero abilities."""
    ACTIVE = "active"
    PASSIVE = "passive"
    ULTIMATE = "ultimate"


@dataclass
class HeroAbility:
    """Definition of a hero ability."""
    id: str
    name: str
    description: str
    ability_type: AbilityType
    unlock_level: int
    max_rank: int
    base_cooldown: float
    mana_cost: int
    icon: str


# Hero abilities (would normally come from data files)
HERO_ABILITIES = {
    "heroic_strike": HeroAbility(
        id="heroic_strike",
        name="Heroic Strike",
        description="A powerful strike that deals bonus damage.",
        ability_type=AbilityType.ACTIVE,
        unlock_level=1,
        max_rank=5,
        base_cooldown=8.0,
        mana_cost=30,
        icon="ability_heroic_strike",
    ),
    "shield_bash": HeroAbility(
        id="shield_bash",
        name="Shield Bash",
        description="Bash the target with your shield, stunning them.",
        ability_type=AbilityType.ACTIVE,
        unlock_level=3,
        max_rank=5,
        base_cooldown=12.0,
        mana_cost=40,
        icon="ability_shield_bash",
    ),
    "battle_cry": HeroAbility(
        id="battle_cry",
        name="Battle Cry",
        description="Inspire nearby allies, increasing their attack damage.",
        ability_type=AbilityType.ACTIVE,
        unlock_level=6,
        max_rank=5,
        base_cooldown=30.0,
        mana_cost=60,
        icon="ability_battle_cry",
    ),
    "toughness": HeroAbility(
        id="toughness",
        name="Toughness",
        description="Passive: Increases armor and health regeneration.",
        ability_type=AbilityType.PASSIVE,
        unlock_level=1,
        max_rank=5,
        base_cooldown=0,
        mana_cost=0,
        icon="ability_toughness",
    ),
    "last_stand": HeroAbility(
        id="last_stand",
        name="Last Stand",
        description="Ultimate: Become invulnerable for a short duration.",
        ability_type=AbilityType.ULTIMATE,
        unlock_level=10,
        max_rank=3,
        base_cooldown=120.0,
        mana_cost=100,
        icon="ability_last_stand",
    ),
}


class HeroProgression:
    """
    Manages hero leveling, stats, and abilities.
    """

    def __init__(self, hero_id: int):
        self.hero_id = hero_id
        self.owner_id = get_owner(hero_id)
        self.level = 1
        self.experience = 0
        self.skill_points = 0
        self.ability_ranks: Dict[str, int] = {}

        # Initialize abilities at rank 0
        for ability_id in HERO_ABILITIES:
            self.ability_ranks[ability_id] = 0

        # Apply initial stats
        self._apply_stats()

    def add_experience(self, amount: int, source: str = "") -> None:
        """
        Add experience to the hero.

        Args:
            amount: Amount of XP to add
            source: Description of XP source (for logging)
        """
        if self.level >= MAX_LEVEL:
            return

        self.experience += amount

        # Show floating XP text
        x, y, z = get_position(self.hero_id)
        spawn_floating_text(x, y + 2.5, z, f"+{amount} XP", "experience")

        log_debug(f"Hero {self.hero_id} gained {amount} XP from {source}")

        # Check for level up(s)
        while self._can_level_up():
            self._level_up()

    def _get_xp_for_level(self, level: int) -> int:
        """Calculate XP required to reach a specific level."""
        if level <= 1:
            return 0

        total_xp = 0
        for lvl in range(2, level + 1):
            xp_for_this_level = int(BASE_XP_TO_LEVEL * (XP_SCALING ** (lvl - 2)))
            total_xp += xp_for_this_level

        return total_xp

    def _get_xp_for_next_level(self) -> int:
        """Get XP required for the next level."""
        if self.level >= MAX_LEVEL:
            return 0
        return int(BASE_XP_TO_LEVEL * (XP_SCALING ** (self.level - 1)))

    def _can_level_up(self) -> bool:
        """Check if hero has enough XP to level up."""
        if self.level >= MAX_LEVEL:
            return False

        xp_needed = self._get_xp_for_next_level()
        current_level_xp = self.experience - self._get_xp_for_level(self.level)

        return current_level_xp >= xp_needed

    def _level_up(self) -> None:
        """Process a level up."""
        self.level += 1
        self.skill_points += 1

        # Update stats
        self._apply_stats()

        # Heal to full on level up
        max_health = self._get_stat("max_health")
        max_mana = self._get_stat("max_mana")
        set_health(self.hero_id, max_health)
        set_mana(self.hero_id, max_mana)

        # Visual and audio effects
        x, y, z = get_position(self.hero_id)
        play_effect("level_up_burst", x, y, z)
        play_sound("level_up_sound", x, y, z)

        # Show level up text
        spawn_floating_text(x, y + 3.0, z, f"Level {self.level}!", "level_up")

        # Check for newly unlocked abilities
        newly_unlocked = self._get_newly_unlocked_abilities()
        for ability in newly_unlocked:
            show_notification(self.owner_id, f"New ability unlocked: {ability.name}", "success")
            play_sound("ability_unlock", x, y, z)

        # Notify player
        show_notification(self.owner_id, f"Hero reached level {self.level}!", "success")

        log_debug(f"Hero {self.hero_id} leveled up to {self.level}")

    def _apply_stats(self) -> None:
        """Apply current stats based on level."""
        for stat_name, base_value in BASE_STATS.items():
            growth = STAT_GROWTH.get(stat_name, 0)
            total_value = base_value + (growth * (self.level - 1))

            # Apply passive ability bonuses
            total_value = self._apply_ability_stat_bonus(stat_name, total_value)

            set_stat(self.hero_id, stat_name, total_value)

    def _get_stat(self, stat_name: str) -> float:
        """Get current stat value."""
        base = BASE_STATS.get(stat_name, 0)
        growth = STAT_GROWTH.get(stat_name, 0)
        total = base + (growth * (self.level - 1))
        return self._apply_ability_stat_bonus(stat_name, total)

    def _apply_ability_stat_bonus(self, stat_name: str, base_value: float) -> float:
        """Apply stat bonuses from passive abilities."""
        # Toughness passive bonus
        toughness_rank = self.ability_ranks.get("toughness", 0)
        if toughness_rank > 0:
            if stat_name == "armor":
                base_value += toughness_rank * 3  # +3 armor per rank
            elif stat_name == "health_regen":
                base_value *= (1.0 + toughness_rank * 0.2)  # +20% per rank

        return base_value

    def _get_newly_unlocked_abilities(self) -> List[HeroAbility]:
        """Get abilities that were just unlocked at current level."""
        unlocked = []
        for ability in HERO_ABILITIES.values():
            if ability.unlock_level == self.level:
                unlocked.append(ability)
        return unlocked

    def get_available_abilities(self) -> List[str]:
        """Get list of ability IDs that are unlocked."""
        available = []
        for ability_id, ability in HERO_ABILITIES.items():
            if ability.unlock_level <= self.level:
                available.append(ability_id)
        return available

    def can_upgrade_ability(self, ability_id: str) -> Tuple[bool, str]:
        """
        Check if an ability can be upgraded.

        Args:
            ability_id: The ability to check

        Returns:
            Tuple of (can_upgrade, reason_if_not)
        """
        if ability_id not in HERO_ABILITIES:
            return False, "Unknown ability"

        ability = HERO_ABILITIES[ability_id]

        # Check unlock level
        if ability.unlock_level > self.level:
            return False, f"Unlocks at level {ability.unlock_level}"

        # Check current rank
        current_rank = self.ability_ranks.get(ability_id, 0)
        if current_rank >= ability.max_rank:
            return False, "Already at max rank"

        # Check skill points
        if self.skill_points <= 0:
            return False, "No skill points available"

        # Ultimate ability requires higher level per rank
        if ability.ability_type == AbilityType.ULTIMATE:
            required_level = ability.unlock_level + (current_rank * 5)
            if self.level < required_level:
                return False, f"Requires level {required_level}"

        return True, ""

    def upgrade_ability(self, ability_id: str) -> bool:
        """
        Spend a skill point to upgrade an ability.

        Args:
            ability_id: The ability to upgrade

        Returns:
            True if upgraded successfully
        """
        can, reason = self.can_upgrade_ability(ability_id)
        if not can:
            show_notification(self.owner_id, f"Cannot upgrade: {reason}", "error")
            return False

        # Spend skill point
        self.skill_points -= 1

        # Increase rank
        self.ability_ranks[ability_id] = self.ability_ranks.get(ability_id, 0) + 1
        new_rank = self.ability_ranks[ability_id]

        # Re-apply stats (for passive abilities)
        self._apply_stats()

        # Effects
        x, y, z = get_position(self.hero_id)
        play_sound("ability_upgrade", x, y, z)

        ability = HERO_ABILITIES[ability_id]
        show_notification(self.owner_id, f"{ability.name} upgraded to rank {new_rank}!", "success")

        log_debug(f"Hero {self.hero_id} upgraded {ability_id} to rank {new_rank}")
        return True

    def get_ability_info(self, ability_id: str) -> Optional[Dict]:
        """Get detailed info about an ability for UI display."""
        if ability_id not in HERO_ABILITIES:
            return None

        ability = HERO_ABILITIES[ability_id]
        current_rank = self.ability_ranks.get(ability_id, 0)

        return {
            "id": ability.id,
            "name": ability.name,
            "description": ability.description,
            "type": ability.ability_type.value,
            "icon": ability.icon,
            "unlock_level": ability.unlock_level,
            "unlocked": ability.unlock_level <= self.level,
            "current_rank": current_rank,
            "max_rank": ability.max_rank,
            "cooldown": self._get_ability_cooldown(ability_id),
            "mana_cost": self._get_ability_mana_cost(ability_id),
            "can_upgrade": self.can_upgrade_ability(ability_id)[0],
        }

    def _get_ability_cooldown(self, ability_id: str) -> float:
        """Get current cooldown for an ability (reduced by rank)."""
        ability = HERO_ABILITIES.get(ability_id)
        if not ability:
            return 0

        rank = self.ability_ranks.get(ability_id, 0)
        if rank == 0:
            return ability.base_cooldown

        # 5% cooldown reduction per rank
        reduction = rank * 0.05
        return ability.base_cooldown * (1.0 - reduction)

    def _get_ability_mana_cost(self, ability_id: str) -> int:
        """Get current mana cost for an ability."""
        ability = HERO_ABILITIES.get(ability_id)
        if not ability:
            return 0

        # Mana cost doesn't change with rank in this example
        return ability.mana_cost

    def get_progression_info(self) -> Dict:
        """Get hero progression info for UI display."""
        current_level_xp = self.experience - self._get_xp_for_level(self.level)
        xp_needed = self._get_xp_for_next_level()

        return {
            "level": self.level,
            "max_level": MAX_LEVEL,
            "experience": self.experience,
            "current_level_xp": current_level_xp,
            "xp_for_next_level": xp_needed,
            "xp_progress": current_level_xp / xp_needed if xp_needed > 0 else 1.0,
            "skill_points": self.skill_points,
            "abilities": [self.get_ability_info(aid) for aid in HERO_ABILITIES.keys()],
        }


# Global storage for hero progression
_hero_progression: Dict[int, HeroProgression] = {}


def on_create(hero_id: int) -> None:
    """Initialize hero progression system."""
    _hero_progression[hero_id] = HeroProgression(hero_id)
    log_debug(f"Hero progression initialized for {hero_id}")


def on_death(hero_id: int, killer_id: int) -> None:
    """Handle hero death - progression persists."""
    # Heroes typically respawn, so we don't delete progression
    # Just log the death
    if hero_id in _hero_progression:
        hero = _hero_progression[hero_id]
        log_debug(f"Hero {hero_id} (level {hero.level}) died")


def on_enemy_killed(hero_id: int, enemy_id: int) -> None:
    """
    Called when the hero kills an enemy.
    Grant experience based on enemy type.
    """
    if hero_id not in _hero_progression:
        return

    hero = _hero_progression[hero_id]

    # Get XP value from enemy
    xp_value = get_property(enemy_id, "xp_value")
    if xp_value is None:
        xp_value = 25  # Default XP

    enemy_type = get_entity_type(enemy_id)
    hero.add_experience(xp_value, f"killing {enemy_type}")


# External API
def add_xp(hero_id: int, amount: int, source: str = "") -> None:
    """Add experience to a hero."""
    if hero_id in _hero_progression:
        _hero_progression[hero_id].add_experience(amount, source)


def get_level(hero_id: int) -> int:
    """Get hero's current level."""
    if hero_id in _hero_progression:
        return _hero_progression[hero_id].level
    return 1


def upgrade_ability(hero_id: int, ability_id: str) -> bool:
    """Upgrade a hero ability."""
    if hero_id in _hero_progression:
        return _hero_progression[hero_id].upgrade_ability(ability_id)
    return False


def get_progression(hero_id: int) -> Optional[Dict]:
    """Get hero progression info."""
    if hero_id in _hero_progression:
        return _hero_progression[hero_id].get_progression_info()
    return None


def get_ability_rank(hero_id: int, ability_id: str) -> int:
    """Get current rank of an ability."""
    if hero_id in _hero_progression:
        return _hero_progression[hero_id].ability_ranks.get(ability_id, 0)
    return 0
