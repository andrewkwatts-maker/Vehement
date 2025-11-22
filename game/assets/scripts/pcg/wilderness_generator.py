# @name: Wilderness Generator
# @version: 1.0.0
# @author: Nova3D Engine Team
# @description: Generates natural wilderness areas with vegetation and wildlife

def generate(ctx):
    """
    Generate wilderness environment with natural features.
    """
    # Phase 1: Base terrain with natural variation
    generate_natural_terrain(ctx)

    # Phase 2: Water features
    generate_water_features(ctx)

    # Phase 3: Vegetation
    generate_vegetation(ctx)

    # Phase 4: Wildlife spawns
    spawn_wildlife(ctx)

    # Phase 5: Points of interest
    generate_poi(ctx)


def preview(ctx):
    """
    Fast preview of wilderness generation.
    """
    for x in range(0, ctx.width, 3):
        for y in range(0, ctx.height, 3):
            if ctx.is_water(x, y):
                tile = "water"
            else:
                biome = ctx.get_biome(x, y)
                if biome == "forest":
                    tile = "forest"
                elif biome == "desert":
                    tile = "dirt"
                elif biome == "mountain":
                    tile = "rocks"
                else:
                    tile = "grass"

            ctx.fill_rect(x, y, 3, 3, tile)


def generate_natural_terrain(ctx):
    """
    Generate natural terrain with procedural variation.
    """
    for y in range(ctx.height):
        for x in range(ctx.width):
            # Skip occupied tiles
            if ctx.is_occupied(x, y):
                continue

            biome = ctx.get_biome(x, y)

            # Use multiple noise octaves for natural look
            noise1 = ctx.perlin(x * 0.05, y * 0.05, 1.0, 3)
            noise2 = ctx.simplex(x * 0.1 + 100, y * 0.1 + 100, 1.0, 2)
            noise3 = ctx.worley(x * 0.03, y * 0.03, 1.0)

            combined = (noise1 * 0.5 + noise2 * 0.3 + noise3 * 0.2)

            # Select tile based on biome and noise
            tile = select_natural_tile(biome, combined, ctx)
            ctx.set_tile(x, y, tile)


def select_natural_tile(biome, noise, ctx):
    """
    Select natural tile based on biome and noise value.
    """
    if biome == "forest":
        if noise > 0.3:
            return "forest2" if ctx.random() < 0.3 else "forest"
        elif noise > 0.0:
            return "grass"
        elif noise > -0.3:
            return "dirt"
        else:
            return "rocks"

    elif biome == "desert":
        if noise > 0.4:
            return "rocks"
        elif noise > 0.0:
            return "dirt"
        else:
            return "dirt"  # Sand variation would go here

    elif biome == "mountain":
        if noise > 0.5:
            return "stone"
        elif noise > 0.2:
            return "rocks"
        elif noise > -0.2:
            return "dirt"
        else:
            return "grass"

    elif biome == "wetland":
        if noise > 0.2:
            return "grass"
        elif noise > -0.2:
            return "dirt"
        else:
            return "water"  # Shallow water/mud

    elif biome == "grassland":
        if noise > 0.3:
            return "grass2"
        elif noise > -0.2:
            return "grass"
        else:
            return "dirt"

    else:
        # Default natural terrain
        if noise > 0.4:
            return "forest"
        elif noise > 0.0:
            return "grass"
        elif noise > -0.3:
            return "grass2"
        else:
            return "dirt"


def generate_water_features(ctx):
    """
    Generate natural water features like streams and ponds.
    """
    # Find low points for ponds
    for y in range(2, ctx.height - 2, 10):
        for x in range(2, ctx.width - 2, 10):
            if ctx.is_occupied(x, y):
                continue

            # Check if this is a low point (potential pond)
            elevation = ctx.get_elevation(x, y)
            is_low = True

            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    if dx == 0 and dy == 0:
                        continue
                    if ctx.in_bounds(x + dx, y + dy):
                        neighbor_elev = ctx.get_elevation(x + dx, y + dy)
                        if neighbor_elev < elevation:
                            is_low = False
                            break

            # Create small pond at low points
            if is_low and ctx.random() < 0.3:
                pond_size = ctx.random_int(2, 5)
                create_pond(ctx, x, y, pond_size)


def create_pond(ctx, center_x, center_y, radius):
    """
    Create a natural-looking pond.
    """
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            px = center_x + dx
            py = center_y + dy

            if not ctx.in_bounds(px, py):
                continue
            if ctx.is_occupied(px, py):
                continue

            # Circular shape with noise
            dist = ctx.distance(center_x, center_y, px, py)
            noise = ctx.perlin(px * 0.2, py * 0.2, 1.0, 1) * 0.5

            if dist + noise < radius:
                ctx.set_tile(px, py, "water")
                ctx.mark_occupied(px, py)


def generate_vegetation(ctx):
    """
    Generate natural vegetation based on biome.
    """
    for y in range(ctx.height):
        for x in range(ctx.width):
            if ctx.is_occupied(x, y):
                continue
            if ctx.is_water(x, y):
                continue

            biome = ctx.get_biome(x, y)

            # Vegetation density varies by biome
            if biome == "forest":
                density = 0.4
                tree_types = ["tree_oak", "tree_pine"]
            elif biome == "grassland":
                density = 0.1
                tree_types = ["tree_oak", "bush"]
            elif biome == "wetland":
                density = 0.15
                tree_types = ["tree_oak", "bush"]
            elif biome == "mountain":
                density = 0.05
                tree_types = ["tree_pine"]
            elif biome == "desert":
                density = 0.02
                tree_types = ["cactus"]
            else:
                density = 0.08
                tree_types = ["tree_oak", "bush"]

            # Use clustering for natural look
            cluster_noise = ctx.worley(x * 0.1, y * 0.1, 1.0)
            adjusted_density = density * (1.0 + cluster_noise)

            if ctx.random() < adjusted_density:
                tree_type = tree_types[ctx.random_int(0, len(tree_types) - 1)]
                scale = 0.8 + ctx.random() * 0.4
                ctx.spawn_foliage(x, y, tree_type, scale)


def spawn_wildlife(ctx):
    """
    Spawn wildlife in wilderness areas.
    """
    # Bird spawn zones - mostly in forests
    for y in range(0, ctx.height, 8):
        for x in range(0, ctx.width, 8):
            biome = ctx.get_biome(x, y)

            if biome in ["forest", "grassland", "wetland"]:
                if not ctx.is_occupied(x, y) and ctx.is_walkable(x, y):
                    if ctx.random() < 0.03:
                        ctx.spawn_entity(x, y, "crow")

    # Ground animals
    for y in range(0, ctx.height, 12):
        for x in range(0, ctx.width, 12):
            biome = ctx.get_biome(x, y)

            if biome in ["forest", "grassland"]:
                if not ctx.is_occupied(x, y) and ctx.is_walkable(x, y):
                    if ctx.random() < 0.02:
                        animal = "deer" if ctx.random() < 0.5 else "rabbit"
                        ctx.spawn_entity(x, y, animal)

    # Dangerous wildlife in remote areas
    for y in range(0, ctx.height, 20):
        for x in range(0, ctx.width, 20):
            biome = ctx.get_biome(x, y)

            # Check if far from civilization
            pop_density = ctx.get_population_density(x, y)
            if pop_density < 10:  # Remote area
                if not ctx.is_occupied(x, y) and ctx.is_walkable(x, y):
                    if ctx.random() < 0.01:
                        ctx.spawn_entity(x, y, "wolf")


def generate_poi(ctx):
    """
    Generate points of interest in wilderness (camps, ruins, etc.)
    """
    poi_count = (ctx.width * ctx.height) // 2000

    for _ in range(poi_count):
        # Find suitable location
        attempts = 0
        while attempts < 50:
            x = ctx.random_int(5, ctx.width - 10)
            y = ctx.random_int(5, ctx.height - 10)

            # Check if area is suitable
            if is_poi_suitable(ctx, x, y, 5, 5):
                poi_type = ctx.random_int(0, 2)

                if poi_type == 0:
                    create_campsite(ctx, x, y)
                elif poi_type == 1:
                    create_ruins(ctx, x, y)
                else:
                    create_clearing(ctx, x, y)
                break

            attempts += 1


def is_poi_suitable(ctx, x, y, w, h):
    """
    Check if area is suitable for POI placement.
    """
    for dy in range(h):
        for dx in range(w):
            px = x + dx
            py = y + dy
            if not ctx.in_bounds(px, py):
                return False
            if ctx.is_occupied(px, py):
                return False
            if ctx.is_water(px, py):
                return False
    return True


def create_campsite(ctx, x, y):
    """
    Create an abandoned campsite.
    """
    # Clear area
    ctx.fill_rect(x, y, 4, 4, "dirt")

    # Mark as occupied
    for dy in range(4):
        for dx in range(4):
            ctx.mark_occupied(x + dx, y + dy)

    # Spawn loot
    ctx.spawn_entity(x + 1, y + 1, "loot_box")
    ctx.mark_zone(x, y, 4, 4, "loot_zone")


def create_ruins(ctx, x, y):
    """
    Create ancient ruins.
    """
    # Stone foundation
    for dy in range(5):
        for dx in range(5):
            if ctx.in_bounds(x + dx, y + dy):
                if dx == 0 or dx == 4 or dy == 0 or dy == 4:
                    ctx.set_wall(x + dx, y + dy, "stone", 1.0)
                else:
                    ctx.set_tile(x + dx, y + dy, "stone")
                ctx.mark_occupied(x + dx, y + dy)

    # Collapsed sections
    for _ in range(3):
        cx = x + ctx.random_int(0, 4)
        cy = y + ctx.random_int(0, 4)
        if ctx.in_bounds(cx, cy):
            ctx.set_tile(cx, cy, "rocks")

    ctx.mark_zone(x, y, 5, 5, "poi_ruins")


def create_clearing(ctx, x, y):
    """
    Create a natural clearing.
    """
    # Grass clearing
    ctx.fill_rect(x, y, 6, 6, "grass2")

    for dy in range(6):
        for dx in range(6):
            ctx.mark_occupied(x + dx, y + dy)

    # Chance for resource spawn
    if ctx.random() < 0.5:
        cx = x + 3
        cy = y + 3
        ctx.spawn_entity(cx, cy, "herb_node")
