# Python Scripting Guide

Nova3D Engine includes a powerful Python scripting system for game logic, AI behaviors, spell effects, and more. This guide covers setting up scripts, available APIs, and best practices.

## Overview

The scripting system provides:

- **Hot Reloading**: Scripts reload automatically when modified
- **Game API Access**: Full access to entities, components, and systems
- **Event System**: Subscribe to game events from Python
- **Type Safety**: Automatic type conversion between C++ and Python
- **Sandboxing**: Optional execution restrictions

## Setting Up Scripts

### Script Locations

Scripts are searched in the following paths:
- `scripts/` - Main scripts directory
- `game/scripts/` - Game-specific scripts
- Custom paths configured in `PythonEngineConfig`

### Basic Script Structure

```python
# scripts/my_script.py

def on_load():
    """Called when script is loaded."""
    print("Script loaded!")

def on_unload():
    """Called when script is unloaded."""
    print("Script unloading...")

def update(dt):
    """Called each frame with delta time."""
    pass
```

### Script Configuration

Configure scripting in C++:

```cpp
Nova::Scripting::PythonEngineConfig config;
config.scriptPaths = {"scripts/", "game/scripts/"};
config.enableHotReload = true;
config.hotReloadCheckInterval = 1.0f;  // Check every second
config.enableSandbox = true;
config.maxExecutionTimeMs = 100;
config.verboseErrors = true;

pythonEngine.Initialize(config);
```

## Available APIs

### Entity API

```python
from nova import entities

# Get entity by handle
entity = entities.get(handle)

# Get entity by name
player = entities.find("Player")

# Get all entities of type
zombies = entities.find_all("Zombie")

# Create entity
new_entity = entities.create("Zombie", {
    "position": [10, 0, 5],
    "health": 100
})

# Destroy entity
entities.destroy(handle)

# Check if entity exists
if entities.exists(handle):
    # Entity is valid
    pass
```

### Component API

```python
from nova import components

# Get component from entity
health = components.get(entity, "HealthComponent")
transform = components.get(entity, "Transform")

# Access component properties
current_health = health.value
max_health = health.max_value

# Modify component
health.value = 50
transform.position = [10, 0, 5]
transform.rotation = [0, 45, 0]  # Euler angles

# Add component
components.add(entity, "EffectComponent", {
    "effect_id": "burning",
    "duration": 5.0
})

# Remove component
components.remove(entity, "EffectComponent")

# Check for component
if components.has(entity, "HealthComponent"):
    pass
```

### Transform API

```python
from nova import transform

# Get position
pos = transform.get_position(entity)
x, y, z = pos

# Set position
transform.set_position(entity, [10, 0, 5])

# Get rotation (quaternion)
rot = transform.get_rotation(entity)

# Set rotation (euler degrees)
transform.set_rotation_euler(entity, [0, 90, 0])

# Get forward vector
forward = transform.get_forward(entity)

# Look at target
transform.look_at(entity, target_position)

# Move entity
transform.translate(entity, [0, 0, 1])  # Move forward

# Rotate entity
transform.rotate(entity, [0, 45, 0])  # Rotate 45 degrees Y
```

### Input API

```python
from nova import input

# Keyboard
if input.is_key_pressed("space"):
    print("Space just pressed!")

if input.is_key_down("w"):
    # W is held
    pass

if input.is_key_released("escape"):
    pass

# Mouse
mouse_pos = input.get_mouse_position()
mouse_delta = input.get_mouse_delta()
scroll = input.get_scroll_delta()

if input.is_mouse_button_down(0):  # Left button
    pass

# Mouse position in world
world_pos = input.screen_to_world(mouse_pos)
```

### Event API

```python
from nova import events

# Subscribe to event
def on_damage_received(event):
    damage = event.get("damage", 0)
    source = event.get("source")
    print(f"Took {damage} damage from {source}")

events.subscribe("OnDamage", on_damage_received)

# Subscribe with priority
events.subscribe("OnDamage", handler, priority=100)

# Publish event
events.publish("OnDamage", {
    "damage": 50,
    "source": attacker_id,
    "damage_type": "fire"
})

# Unsubscribe
events.unsubscribe("OnDamage", on_damage_received)

# Queue delayed event
events.queue("OnExplosion", {"position": [10, 0, 5]}, delay=2.0)
```

### Time API

```python
from nova import time

# Get delta time
dt = time.delta_time()

# Get total time
total = time.total_time()

# Get frame count
frame = time.frame_count()

# Set time scale
time.set_scale(0.5)  # Slow motion

# Get time scale
scale = time.get_scale()
```

### Math API

```python
from nova import math

# Vector operations
v1 = [1, 0, 0]
v2 = [0, 1, 0]

length = math.length(v1)
normalized = math.normalize(v1)
dot = math.dot(v1, v2)
cross = math.cross(v1, v2)
lerped = math.lerp(v1, v2, 0.5)

# Distance
dist = math.distance([0, 0, 0], [10, 0, 0])

# Random
value = math.random()  # 0-1
value = math.random_range(10, 20)
point = math.random_point_in_sphere(radius=5)

# Angles
radians = math.deg_to_rad(90)
degrees = math.rad_to_deg(3.14159)
```

### Audio API

```python
from nova import audio

# Play sound
audio.play("sounds/explosion.wav")

# Play at position
audio.play_at("sounds/gunshot.wav", position=[10, 0, 5])

# Play with settings
audio.play("sounds/music.ogg", {
    "volume": 0.8,
    "pitch": 1.0,
    "loop": True
})

# Stop sound
handle = audio.play("sounds/ambient.wav")
audio.stop(handle)

# Fade out
audio.fade_out(handle, duration=2.0)
```

### Debug API

```python
from nova import debug

# Logging
debug.log("Info message")
debug.warn("Warning message")
debug.error("Error message")

# Draw debug shapes (visible in editor)
debug.draw_line(start, end, color=[1, 0, 0])
debug.draw_sphere(center, radius, color=[0, 1, 0])
debug.draw_box(center, size, color=[0, 0, 1])
debug.draw_ray(origin, direction, length=10)

# Draw text in world
debug.draw_text_3d("Hello", position=[0, 2, 0])

# Draw text on screen
debug.draw_text_2d("FPS: 60", position=[10, 10])
```

## Event Binding

### Binding Events to Scripts

Create event bindings in JSON:

```json
{
    "id": "on_player_damage",
    "name": "Player Damage Handler",
    "condition": {
        "eventName": "OnDamage",
        "sourceType": "Player"
    },
    "callbackType": "Python",
    "pythonModule": "combat",
    "pythonFunction": "handle_player_damage",
    "enabled": true
}
```

Handler in Python:

```python
# scripts/combat.py

def handle_player_damage(event):
    damage = event.get("damage", 0)
    player = event.get("source")

    # Apply damage reduction
    if has_armor(player):
        damage *= 0.5

    # Apply damage
    apply_damage(player, damage)

    # Trigger effects
    if damage > 50:
        events.publish("OnHeavyDamage", {"player": player})
```

### Conditional Bindings

```json
{
    "id": "low_health_warning",
    "condition": {
        "eventName": "OnHealthChanged",
        "propertyConditions": [
            {
                "property": "health",
                "operator": "less_than",
                "value": 25
            }
        ]
    },
    "pythonFunction": "show_low_health_warning"
}
```

## AI Behaviors

### Basic AI Script

```python
# scripts/ai/zombie_ai.py

from nova import entities, transform, math, time

class ZombieAI:
    def __init__(self, entity):
        self.entity = entity
        self.target = None
        self.attack_range = 2.0
        self.move_speed = 3.0
        self.attack_cooldown = 0

    def update(self, dt):
        self.attack_cooldown = max(0, self.attack_cooldown - dt)

        # Find target
        if not self.target or not entities.exists(self.target):
            self.target = self.find_nearest_player()

        if not self.target:
            self.wander()
            return

        # Get distance to target
        my_pos = transform.get_position(self.entity)
        target_pos = transform.get_position(self.target)
        distance = math.distance(my_pos, target_pos)

        if distance <= self.attack_range:
            self.attack()
        else:
            self.move_towards(target_pos)

    def find_nearest_player(self):
        players = entities.find_all("Player")
        if not players:
            return None

        my_pos = transform.get_position(self.entity)
        nearest = None
        nearest_dist = float('inf')

        for player in players:
            dist = math.distance(my_pos, transform.get_position(player))
            if dist < nearest_dist:
                nearest_dist = dist
                nearest = player

        return nearest

    def move_towards(self, target_pos):
        my_pos = transform.get_position(self.entity)
        direction = math.normalize([
            target_pos[0] - my_pos[0],
            0,
            target_pos[2] - my_pos[2]
        ])

        dt = time.delta_time()
        new_pos = [
            my_pos[0] + direction[0] * self.move_speed * dt,
            my_pos[1],
            my_pos[2] + direction[2] * self.move_speed * dt
        ]

        transform.set_position(self.entity, new_pos)
        transform.look_at(self.entity, target_pos)

    def attack(self):
        if self.attack_cooldown > 0:
            return

        events.publish("OnAttack", {
            "attacker": self.entity,
            "target": self.target,
            "damage": 10
        })

        self.attack_cooldown = 1.0

    def wander(self):
        # Random wandering logic
        pass

# Global AI instances
_ais = {}

def on_spawn(entity):
    """Called when zombie spawns."""
    _ais[entity] = ZombieAI(entity)

def on_update(entity, dt):
    """Called each tick."""
    if entity in _ais:
        _ais[entity].update(dt)

def on_destroy(entity):
    """Called when zombie dies."""
    if entity in _ais:
        del _ais[entity]
```

### State Machine AI

```python
# scripts/ai/state_machine.py

class State:
    def enter(self, ai):
        pass

    def update(self, ai, dt):
        pass

    def exit(self, ai):
        pass

class IdleState(State):
    def __init__(self):
        self.idle_time = 0

    def enter(self, ai):
        self.idle_time = 0
        # Play idle animation
        ai.play_animation("idle")

    def update(self, ai, dt):
        self.idle_time += dt

        # Check for threats
        if ai.detect_enemy():
            return "chase"

        # Patrol after idling
        if self.idle_time > 3.0:
            return "patrol"

        return None

class ChaseState(State):
    def enter(self, ai):
        ai.play_animation("run")

    def update(self, ai, dt):
        target = ai.get_target()
        if not target:
            return "idle"

        distance = ai.distance_to(target)

        if distance < ai.attack_range:
            return "attack"

        ai.move_towards(target)
        return None

class AttackState(State):
    def __init__(self):
        self.attack_timer = 0

    def enter(self, ai):
        self.attack_timer = 0
        ai.play_animation("attack")

    def update(self, ai, dt):
        self.attack_timer += dt

        if self.attack_timer > 0.5:  # Attack animation done
            ai.deal_damage()

            target = ai.get_target()
            if not target:
                return "idle"

            distance = ai.distance_to(target)
            if distance > ai.attack_range:
                return "chase"

            self.attack_timer = 0

        return None

class StateMachineAI:
    def __init__(self, entity):
        self.entity = entity
        self.states = {
            "idle": IdleState(),
            "chase": ChaseState(),
            "attack": AttackState(),
            "patrol": PatrolState()
        }
        self.current_state = "idle"
        self.states[self.current_state].enter(self)

    def update(self, dt):
        next_state = self.states[self.current_state].update(self, dt)
        if next_state and next_state != self.current_state:
            self.transition_to(next_state)

    def transition_to(self, state_name):
        self.states[self.current_state].exit(self)
        self.current_state = state_name
        self.states[self.current_state].enter(self)
```

## Spell Effects

### Basic Spell

```python
# scripts/spells/fireball.py

from nova import entities, transform, math, events, debug

def cast(caster, target_pos):
    """Cast fireball at target position."""

    # Get caster position
    caster_pos = transform.get_position(caster)

    # Calculate direction
    direction = math.normalize([
        target_pos[0] - caster_pos[0],
        target_pos[1] - caster_pos[1],
        target_pos[2] - caster_pos[2]
    ])

    # Create projectile
    fireball = entities.create("Projectile", {
        "position": caster_pos,
        "velocity": [d * 20 for d in direction],
        "damage": 50,
        "damage_type": "fire",
        "on_hit": "on_fireball_hit",
        "lifetime": 5.0
    })

    # Play cast sound
    audio.play_at("sounds/fireball_cast.wav", caster_pos)

    # Spawn cast particles
    events.publish("SpawnParticles", {
        "effect": "fire_cast",
        "position": caster_pos
    })

    return fireball

def on_fireball_hit(projectile, target):
    """Called when fireball hits something."""

    pos = transform.get_position(projectile)

    # Area damage
    nearby = entities.query_sphere(pos, radius=3.0)
    for entity in nearby:
        if entities.has_component(entity, "HealthComponent"):
            damage = calculate_damage(projectile, entity)
            events.publish("OnDamage", {
                "target": entity,
                "damage": damage,
                "type": "fire",
                "source": projectile
            })

    # Explosion effect
    events.publish("SpawnParticles", {
        "effect": "fire_explosion",
        "position": pos
    })

    # Explosion sound
    audio.play_at("sounds/explosion.wav", pos)

    # Destroy projectile
    entities.destroy(projectile)

def calculate_damage(projectile, target):
    """Calculate damage with falloff."""
    proj_pos = transform.get_position(projectile)
    target_pos = transform.get_position(target)
    distance = math.distance(proj_pos, target_pos)

    base_damage = 50
    falloff = max(0, 1 - distance / 3.0)

    return base_damage * falloff
```

### Buff/Debuff System

```python
# scripts/effects/buffs.py

class Buff:
    def __init__(self, target, duration):
        self.target = target
        self.duration = duration
        self.elapsed = 0

    def apply(self):
        """Apply buff effects."""
        pass

    def update(self, dt):
        """Update buff each frame."""
        self.elapsed += dt
        return self.elapsed < self.duration

    def remove(self):
        """Remove buff effects."""
        pass

class SpeedBuff(Buff):
    def __init__(self, target, duration, multiplier=1.5):
        super().__init__(target, duration)
        self.multiplier = multiplier
        self.original_speed = None

    def apply(self):
        stats = components.get(self.target, "Stats")
        self.original_speed = stats.move_speed
        stats.move_speed *= self.multiplier

        events.publish("BuffApplied", {
            "target": self.target,
            "buff": "speed",
            "duration": self.duration
        })

    def remove(self):
        stats = components.get(self.target, "Stats")
        stats.move_speed = self.original_speed

        events.publish("BuffRemoved", {
            "target": self.target,
            "buff": "speed"
        })

class DamageOverTime(Buff):
    def __init__(self, target, duration, damage_per_second):
        super().__init__(target, duration)
        self.dps = damage_per_second
        self.tick_timer = 0

    def update(self, dt):
        self.tick_timer += dt

        if self.tick_timer >= 1.0:
            self.tick_timer -= 1.0
            events.publish("OnDamage", {
                "target": self.target,
                "damage": self.dps,
                "type": "dot"
            })

        return super().update(dt)
```

## Best Practices

### Performance

1. **Cache References**: Store entity handles instead of querying repeatedly
2. **Use Events**: Prefer event-driven logic over polling
3. **Minimize Allocations**: Reuse lists and dictionaries
4. **Batch Operations**: Group similar operations together

```python
# Good - cached reference
class AI:
    def __init__(self, entity):
        self.entity = entity
        self.transform = components.get(entity, "Transform")

    def update(self, dt):
        pos = self.transform.position  # Fast

# Bad - query every frame
def update(entity, dt):
    transform = components.get(entity, "Transform")  # Slow
```

### Error Handling

```python
def safe_cast(caster, target):
    try:
        if not entities.exists(caster):
            return False

        if not has_enough_mana(caster):
            events.publish("CastFailed", {"reason": "no_mana"})
            return False

        do_cast(caster, target)
        return True

    except Exception as e:
        debug.error(f"Cast failed: {e}")
        return False
```

### Code Organization

```
scripts/
├── ai/
│   ├── __init__.py
│   ├── base_ai.py
│   ├── zombie_ai.py
│   └── boss_ai.py
├── spells/
│   ├── __init__.py
│   ├── base_spell.py
│   ├── fireball.py
│   └── heal.py
├── effects/
│   ├── buffs.py
│   └── debuffs.py
├── systems/
│   ├── combat.py
│   └── inventory.py
└── utils/
    ├── math_utils.py
    └── entity_utils.py
```

### Documentation

```python
def cast_spell(caster, spell_id, target):
    """
    Cast a spell from caster to target.

    Args:
        caster: Entity handle of the caster
        spell_id: String ID of the spell to cast
        target: Entity handle or position to target

    Returns:
        bool: True if spell was cast successfully

    Raises:
        ValueError: If spell_id is invalid
    """
    pass
```

## Examples

### Complete AI Example

See `scripts/ai/zombie_ai.py` above for a full AI implementation.

### Complete Spell Example

See `scripts/spells/fireball.py` above for a full spell implementation.

### Event-Driven Game Logic

```python
# scripts/game_logic.py

from nova import events, entities, components

def setup():
    """Initialize game logic event handlers."""
    events.subscribe("OnDamage", handle_damage)
    events.subscribe("OnDeath", handle_death)
    events.subscribe("OnLevelUp", handle_level_up)

def handle_damage(event):
    target = event.get("target")
    damage = event.get("damage", 0)
    damage_type = event.get("type", "physical")

    # Get health component
    health = components.get(target, "HealthComponent")
    if not health:
        return

    # Apply resistances
    final_damage = apply_resistances(target, damage, damage_type)

    # Apply damage
    health.value -= final_damage

    # Check death
    if health.value <= 0:
        events.publish("OnDeath", {
            "entity": target,
            "killer": event.get("source")
        })

def handle_death(event):
    entity = event.get("entity")
    killer = event.get("killer")

    # Drop loot
    drop_loot(entity)

    # Award XP
    if killer and entities.has_component(killer, "ExperienceComponent"):
        xp = get_xp_value(entity)
        events.publish("AddExperience", {
            "entity": killer,
            "amount": xp
        })

    # Destroy entity
    entities.destroy(entity, delay=2.0)  # Delay for death animation

def handle_level_up(event):
    entity = event.get("entity")
    new_level = event.get("level")

    # Increase stats
    stats = components.get(entity, "Stats")
    stats.max_health += 10
    stats.damage += 2

    # Heal to full
    health = components.get(entity, "HealthComponent")
    health.value = stats.max_health

    # Play effect
    events.publish("SpawnParticles", {
        "effect": "level_up",
        "position": transform.get_position(entity)
    })
```
