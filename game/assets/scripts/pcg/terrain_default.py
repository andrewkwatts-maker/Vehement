# @name: Default Terrain Generator
# @version: 1.0.0
# @author: Nova3D Engine Team
# @description: Basic terrain generation with biome-aware tile placement

def generate(ctx):
    """
    Main terrain generation function.

    Generates base terrain tiles based on:
    - Real-world elevation data (if available)
    - Biome classification
    - Water body detection
    - Procedural noise for variation
    """
    for x in range(ctx.width):
        for y in range(ctx.height):
            # Get real-world data
            elevation = ctx.get_elevation(x, y)
            biome = ctx.get_biome(x, y)

            # Check for water first
            if ctx.is_water(x, y):
                ctx.set_tile(x, y, "water")
                ctx.mark_occupied(x, y)
                continue

            # Check for roads (roads take priority)
            if ctx.is_road(x, y):
                road_type = ctx.get_road_type(x, y)
                ctx.set_tile(x, y, road_type)
                ctx.mark_occupied(x, y)
                continue

            # Select tile based on biome
            tile = select_biome_tile(ctx, x, y, biome, elevation)
            ctx.set_tile(x, y, tile)

            # Store elevation for later stages
            ctx.set_elevation(x, y, elevation)

            # Spawn foliage in appropriate biomes
            if biome == "forest":
                if ctx.random() < 0.3:
                    ctx.spawn_foliage(x, y, "tree_oak")
            elif biome == "park":
                if ctx.random() < 0.2:
                    ctx.spawn_foliage(x, y, "tree_oak" if ctx.random() < 0.7 else "bush")


def preview(ctx):
    """
    Fast preview generation (lower detail).
    Uses every other tile for speed.
    """
    for x in range(0, ctx.width, 2):
        for y in range(0, ctx.height, 2):
            biome = ctx.get_biome(x, y)

            if ctx.is_water(x, y):
                tile = "water"
            elif ctx.is_road(x, y):
                tile = "asphalt"
            elif biome == "forest":
                tile = "forest"
            elif biome == "urban" or biome == "commercial":
                tile = "concrete"
            else:
                tile = "grass"

            # Fill 2x2 area
            ctx.fill_rect(x, y, 2, 2, tile)


def select_biome_tile(ctx, x, y, biome, elevation):
    """
    Select appropriate tile for the given biome and elevation.
    """
    # Add some noise for variation
    noise = ctx.perlin(x * 0.1, y * 0.1, 1.0, 2)

    if biome == "forest":
        if noise > 0.3:
            return "forest"
        elif noise > -0.2:
            return "grass"
        else:
            return "dirt"

    elif biome == "desert":
        if noise > 0.2:
            return "rocks"
        else:
            return "dirt"

    elif biome == "grassland" or biome == "rural":
        if noise > 0.4:
            return "grass2" if ctx.random() < 0.5 else "grass"
        elif noise > -0.3:
            return "grass"
        else:
            return "dirt"

    elif biome == "mountain":
        if elevation > 0.7:
            return "rocks"
        elif elevation > 0.4:
            return "stone"
        else:
            return "dirt"

    elif biome == "wetland":
        if noise > 0.3:
            return "grass"
        else:
            return "dirt"  # Muddy areas

    elif biome == "urban" or biome == "commercial" or biome == "industrial":
        # Urban areas will have buildings placed later
        return "grass"

    elif biome == "residential" or biome == "suburban":
        return "grass"

    elif biome == "park":
        if noise > 0.2:
            return "grass2"
        else:
            return "grass"

    else:
        # Default grass with variation
        return "grass2" if ctx.random() < 0.3 else "grass"
