"""
Procedural Terrain Generator for Vehement2

Generates terrain features procedurally:
- Height maps using noise functions
- Resource node placement
- Building zone identification
- Path/road generation
- Environmental hazard zones

This script demonstrates:
- PCG (Procedural Content Generation) in Python
- Noise-based terrain generation
- Spatial distribution algorithms
"""

import nova_engine as nova
import math
import random

# ============================================================================
# Constants
# ============================================================================

# Terrain types
class TerrainType:
    GRASS = 0
    DIRT = 1
    SAND = 2
    STONE = 3
    WATER = 4
    FOREST = 5
    SWAMP = 6
    ROAD = 7

# Resource types that can spawn
RESOURCE_TYPES = ["Wood", "Stone", "Metal", "Food"]

# Noise parameters
NOISE_OCTAVES = 4
NOISE_PERSISTENCE = 0.5
NOISE_LACUNARITY = 2.0

# Generation parameters
DEFAULT_SEED = 12345
CHUNK_SIZE = 32  # Tiles per chunk

# ============================================================================
# Noise Functions
# ============================================================================

def hash_2d(x, y, seed):
    """Simple hash function for 2D coordinates."""
    n = x * 374761393 + y * 668265263 + seed * 1013904223
    n = (n ^ (n >> 13)) * 1274126177
    return n ^ (n >> 16)

def smooth_noise_2d(x, y, seed):
    """Generate smooth noise at integer coordinates."""
    corners = (
        hash_2d(x - 1, y - 1, seed) +
        hash_2d(x + 1, y - 1, seed) +
        hash_2d(x - 1, y + 1, seed) +
        hash_2d(x + 1, y + 1, seed)
    ) / 16.0

    sides = (
        hash_2d(x - 1, y, seed) +
        hash_2d(x + 1, y, seed) +
        hash_2d(x, y - 1, seed) +
        hash_2d(x, y + 1, seed)
    ) / 8.0

    center = hash_2d(x, y, seed) / 4.0

    return (corners + sides + center) / 0xFFFFFFFF

def interpolate(a, b, t):
    """Cosine interpolation."""
    ft = t * 3.14159265
    f = (1.0 - math.cos(ft)) * 0.5
    return a * (1.0 - f) + b * f

def interpolated_noise_2d(x, y, seed):
    """Get interpolated noise at any coordinate."""
    int_x = int(x)
    int_y = int(y)
    frac_x = x - int_x
    frac_y = y - int_y

    v1 = smooth_noise_2d(int_x, int_y, seed)
    v2 = smooth_noise_2d(int_x + 1, int_y, seed)
    v3 = smooth_noise_2d(int_x, int_y + 1, seed)
    v4 = smooth_noise_2d(int_x + 1, int_y + 1, seed)

    i1 = interpolate(v1, v2, frac_x)
    i2 = interpolate(v3, v4, frac_x)

    return interpolate(i1, i2, frac_y)

def perlin_noise_2d(x, y, seed, octaves=NOISE_OCTAVES,
                   persistence=NOISE_PERSISTENCE, lacunarity=NOISE_LACUNARITY):
    """Generate multi-octave Perlin noise."""
    total = 0.0
    frequency = 1.0
    amplitude = 1.0
    max_value = 0.0

    for _ in range(octaves):
        total += interpolated_noise_2d(x * frequency, y * frequency, seed) * amplitude
        max_value += amplitude
        amplitude *= persistence
        frequency *= lacunarity

    return total / max_value

# ============================================================================
# Terrain Generation
# ============================================================================

def generate_heightmap(width, height, seed=DEFAULT_SEED, scale=0.05):
    """
    Generate a height map for terrain.

    Args:
        width: Width in tiles
        height: Height in tiles
        seed: Random seed
        scale: Noise scale (smaller = smoother)

    Returns:
        2D list of height values (0.0 to 1.0)
    """
    heightmap = []

    for y in range(height):
        row = []
        for x in range(width):
            # Base terrain height
            height_val = perlin_noise_2d(x * scale, y * scale, seed)

            # Add some variation
            detail = perlin_noise_2d(x * scale * 4, y * scale * 4, seed + 1000) * 0.2
            height_val = (height_val + detail) / 1.2

            # Clamp to 0-1
            height_val = max(0.0, min(1.0, height_val))
            row.append(height_val)

        heightmap.append(row)

    nova.log_info(f"Generated heightmap {width}x{height} with seed {seed}")
    return heightmap

def generate_terrain_types(heightmap, moisture_seed=None):
    """
    Generate terrain types based on height and moisture.

    Args:
        heightmap: 2D height map
        moisture_seed: Seed for moisture noise (optional)

    Returns:
        2D list of terrain type IDs
    """
    height = len(heightmap)
    width = len(heightmap[0]) if height > 0 else 0

    if moisture_seed is None:
        moisture_seed = random.randint(0, 999999)

    terrain_types = []

    for y in range(height):
        row = []
        for x in range(width):
            h = heightmap[y][x]

            # Generate moisture value
            m = perlin_noise_2d(x * 0.03, y * 0.03, moisture_seed)

            # Determine terrain type
            terrain = determine_terrain_type(h, m)
            row.append(terrain)

        terrain_types.append(row)

    nova.log_info(f"Generated terrain types {width}x{height}")
    return terrain_types

def determine_terrain_type(height, moisture):
    """Determine terrain type from height and moisture."""
    # Water at low heights
    if height < 0.2:
        return TerrainType.WATER

    # Swamp at low height with high moisture
    if height < 0.35 and moisture > 0.6:
        return TerrainType.SWAMP

    # Sand at beach level
    if height < 0.3:
        return TerrainType.SAND

    # Stone at high elevations
    if height > 0.75:
        return TerrainType.STONE

    # Forest with high moisture
    if moisture > 0.55 and height > 0.35:
        return TerrainType.FOREST

    # Dirt in dry areas
    if moisture < 0.35:
        return TerrainType.DIRT

    # Default to grass
    return TerrainType.GRASS

# ============================================================================
# Resource Placement
# ============================================================================

def generate_resource_nodes(terrain_types, seed=DEFAULT_SEED):
    """
    Generate resource node positions based on terrain.

    Args:
        terrain_types: 2D terrain type map
        seed: Random seed

    Returns:
        List of (x, y, resource_type) tuples
    """
    random.seed(seed)

    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    resources = []

    for y in range(height):
        for x in range(width):
            terrain = terrain_types[y][x]

            # Check if this tile can spawn resources
            resource = try_spawn_resource(x, y, terrain, seed)
            if resource:
                resources.append((x, y, resource))

    nova.log_info(f"Generated {len(resources)} resource nodes")
    return resources

def try_spawn_resource(x, y, terrain, seed):
    """Try to spawn a resource at this location."""
    # Use position-based random for consistency
    rand_val = (hash_2d(x, y, seed) & 0xFFFF) / 0xFFFF

    if terrain == TerrainType.FOREST:
        # Wood in forests (15% chance)
        if rand_val < 0.15:
            return "Wood"

    elif terrain == TerrainType.STONE:
        # Stone on rocky terrain (20% chance)
        if rand_val < 0.20:
            return "Stone"
        # Metal in mountains (5% chance)
        elif rand_val < 0.25:
            return "Metal"

    elif terrain == TerrainType.GRASS:
        # Food (berries, etc.) in grass (3% chance)
        if rand_val < 0.03:
            return "Food"

    elif terrain == TerrainType.DIRT:
        # Stone occasionally (5% chance)
        if rand_val < 0.05:
            return "Stone"

    return None

# ============================================================================
# Building Zone Generation
# ============================================================================

def generate_building_zones(terrain_types, heightmap):
    """
    Identify valid building zones on the terrain.

    Args:
        terrain_types: 2D terrain type map
        heightmap: 2D height map

    Returns:
        2D bool map (True = can build)
    """
    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    buildable = []

    for y in range(height):
        row = []
        for x in range(width):
            terrain = terrain_types[y][x]
            h = heightmap[y][x]

            # Check if buildable
            can_build = is_buildable(terrain, h, x, y, heightmap)
            row.append(can_build)

        buildable.append(row)

    # Count buildable tiles
    buildable_count = sum(sum(row) for row in buildable)
    nova.log_info(f"Generated building zones: {buildable_count} buildable tiles")

    return buildable

def is_buildable(terrain, height, x, y, heightmap):
    """Check if a tile is buildable."""
    # Can't build on water or swamp
    if terrain in [TerrainType.WATER, TerrainType.SWAMP]:
        return False

    # Can't build on very steep terrain
    if height > 0.85:
        return False

    # Check slope (height difference with neighbors)
    max_slope = 0.0
    for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
        nx, ny = x + dx, y + dy
        if 0 <= ny < len(heightmap) and 0 <= nx < len(heightmap[0]):
            neighbor_height = heightmap[ny][nx]
            slope = abs(height - neighbor_height)
            max_slope = max(max_slope, slope)

    # Too steep to build
    if max_slope > 0.15:
        return False

    return True

# ============================================================================
# Path Generation
# ============================================================================

def generate_paths(terrain_types, start_points, end_points, seed=DEFAULT_SEED):
    """
    Generate paths/roads between points.

    Args:
        terrain_types: 2D terrain type map
        start_points: List of (x, y) start positions
        end_points: List of (x, y) end positions
        seed: Random seed

    Returns:
        List of path tiles (x, y)
    """
    path_tiles = set()

    for start in start_points:
        for end in end_points:
            path = find_path(terrain_types, start, end)
            path_tiles.update(path)

    # Add some random path width variation
    random.seed(seed)
    expanded_path = set()
    for x, y in path_tiles:
        expanded_path.add((x, y))
        # Occasionally widen the path
        if random.random() < 0.3:
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nx, ny = x + dx, y + dy
                if is_valid_path_tile(terrain_types, nx, ny):
                    expanded_path.add((nx, ny))

    nova.log_info(f"Generated {len(expanded_path)} path tiles")
    return list(expanded_path)

def find_path(terrain_types, start, end):
    """Simple A* pathfinding for road generation."""
    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    # Simplified direct path for now
    path = []
    x, y = start
    end_x, end_y = end

    while (x, y) != (end_x, end_y):
        path.append((x, y))

        # Move toward goal
        if x < end_x:
            x += 1
        elif x > end_x:
            x -= 1

        if y < end_y:
            y += 1
        elif y > end_y:
            y -= 1

        # Stay in bounds
        x = max(0, min(width - 1, x))
        y = max(0, min(height - 1, y))

    path.append((end_x, end_y))
    return path

def is_valid_path_tile(terrain_types, x, y):
    """Check if a tile can be part of a path."""
    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    if not (0 <= x < width and 0 <= y < height):
        return False

    terrain = terrain_types[y][x]
    return terrain not in [TerrainType.WATER, TerrainType.SWAMP]

# ============================================================================
# Hazard Zone Generation
# ============================================================================

def generate_hazard_zones(terrain_types, seed=DEFAULT_SEED):
    """
    Generate environmental hazard zones.

    Args:
        terrain_types: 2D terrain type map
        seed: Random seed

    Returns:
        List of hazard zones: (center_x, center_y, radius, hazard_type)
    """
    random.seed(seed)

    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    hazards = []

    # Generate some random hazard zones
    num_hazards = (width * height) // 500  # Roughly 1 per 500 tiles

    for _ in range(num_hazards):
        x = random.randint(0, width - 1)
        y = random.randint(0, height - 1)

        terrain = terrain_types[y][x]

        # Hazard type based on terrain
        if terrain == TerrainType.SWAMP:
            hazards.append((x, y, random.randint(3, 6), "toxic_gas"))
        elif terrain == TerrainType.FOREST:
            if random.random() < 0.3:
                hazards.append((x, y, random.randint(2, 4), "spider_nest"))
        elif terrain == TerrainType.STONE:
            if random.random() < 0.2:
                hazards.append((x, y, random.randint(3, 5), "rockfall"))

    nova.log_info(f"Generated {len(hazards)} hazard zones")
    return hazards

# ============================================================================
# Main Generation Function
# ============================================================================

def generate_terrain(width, height, seed=None):
    """
    Generate complete terrain data.

    Args:
        width: Terrain width in tiles
        height: Terrain height in tiles
        seed: Random seed (optional)

    Returns:
        Dictionary with all terrain data
    """
    if seed is None:
        seed = random.randint(0, 999999)

    nova.log_info(f"Generating terrain {width}x{height} with seed {seed}")

    # Generate heightmap
    heightmap = generate_heightmap(width, height, seed)

    # Generate terrain types
    terrain_types = generate_terrain_types(heightmap, seed + 5000)

    # Generate resources
    resources = generate_resource_nodes(terrain_types, seed + 10000)

    # Generate building zones
    building_zones = generate_building_zones(terrain_types, heightmap)

    # Generate hazard zones
    hazards = generate_hazard_zones(terrain_types, seed + 20000)

    # Find good spawn points (flat grass areas)
    spawn_points = find_spawn_points(terrain_types, heightmap, 5)

    # Generate paths between spawn points
    paths = []
    if len(spawn_points) >= 2:
        paths = generate_paths(terrain_types, spawn_points[:2], spawn_points[2:], seed)

    result = {
        "width": width,
        "height": height,
        "seed": seed,
        "heightmap": heightmap,
        "terrain_types": terrain_types,
        "resources": resources,
        "building_zones": building_zones,
        "hazards": hazards,
        "spawn_points": spawn_points,
        "paths": paths,
    }

    nova.log_info(f"Terrain generation complete. {len(resources)} resources, "
                  f"{len(hazards)} hazards, {len(spawn_points)} spawn points")

    return result

def find_spawn_points(terrain_types, heightmap, count):
    """Find suitable spawn points on the map."""
    height = len(terrain_types)
    width = len(terrain_types[0]) if height > 0 else 0

    candidates = []

    for y in range(height):
        for x in range(width):
            terrain = terrain_types[y][x]
            h = heightmap[y][x]

            # Good spawn: grass, moderate height, flat
            if terrain == TerrainType.GRASS and 0.3 < h < 0.6:
                score = 1.0 - abs(h - 0.45)  # Prefer middle heights
                candidates.append((x, y, score))

    # Sort by score and return top candidates
    candidates.sort(key=lambda c: c[2], reverse=True)

    # Space them out
    spawn_points = []
    min_distance = min(width, height) // 4

    for x, y, score in candidates:
        # Check distance from existing points
        too_close = False
        for sx, sy in spawn_points:
            dist = math.sqrt((x - sx) ** 2 + (y - sy) ** 2)
            if dist < min_distance:
                too_close = True
                break

        if not too_close:
            spawn_points.append((x, y))
            if len(spawn_points) >= count:
                break

    return spawn_points

# ============================================================================
# Initialization
# ============================================================================

def init():
    """Initialize the terrain generator module."""
    nova.log_info("Terrain generator initialized")
    return "success"
