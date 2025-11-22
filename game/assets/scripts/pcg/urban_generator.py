# @name: Urban Area Generator
# @version: 1.0.0
# @author: Nova3D Engine Team
# @description: Generates urban areas with buildings, roads, and city infrastructure

def generate(ctx):
    """
    Generate urban environment with buildings and infrastructure.
    """
    # Phase 1: Identify urban zones
    urban_zones = find_urban_zones(ctx)

    # Phase 2: Generate road network
    generate_road_network(ctx, urban_zones)

    # Phase 3: Place buildings in urban zones
    place_buildings(ctx, urban_zones)

    # Phase 4: Add urban details
    add_urban_details(ctx)

    # Phase 5: Spawn NPCs and entities
    spawn_urban_entities(ctx)


def preview(ctx):
    """
    Fast preview of urban generation.
    """
    # Simple preview - just show zone types
    for x in range(0, ctx.width, 4):
        for y in range(0, ctx.height, 4):
            biome = ctx.get_biome(x, y)

            if biome in ["urban", "commercial", "industrial"]:
                ctx.fill_rect(x, y, 4, 4, "concrete")
            elif ctx.is_road(x, y):
                ctx.fill_rect(x, y, 4, 4, "asphalt")
            else:
                ctx.fill_rect(x, y, 4, 4, "grass")


def find_urban_zones(ctx):
    """
    Find all urban areas in the context.
    Returns list of zone rectangles: (x, y, width, height, type)
    """
    zones = []
    visited = set()

    for y in range(ctx.height):
        for x in range(ctx.width):
            if (x, y) in visited:
                continue

            biome = ctx.get_biome(x, y)
            if biome in ["urban", "commercial", "industrial", "residential"]:
                # Flood fill to find zone extent
                zone = flood_fill_zone(ctx, x, y, biome, visited)
                if zone:
                    zones.append(zone)

    return zones


def flood_fill_zone(ctx, start_x, start_y, zone_type, visited):
    """
    Flood fill to find zone boundaries.
    """
    min_x, max_x = start_x, start_x
    min_y, max_y = start_y, start_y

    stack = [(start_x, start_y)]

    while stack:
        x, y = stack.pop()

        if (x, y) in visited:
            continue
        if not ctx.in_bounds(x, y):
            continue

        biome = ctx.get_biome(x, y)
        if biome != zone_type:
            continue

        visited.add((x, y))
        min_x = min(min_x, x)
        max_x = max(max_x, x)
        min_y = min(min_y, y)
        max_y = max(max_y, y)

        # Add neighbors
        stack.append((x + 1, y))
        stack.append((x - 1, y))
        stack.append((x, y + 1))
        stack.append((x, y - 1))

    width = max_x - min_x + 1
    height = max_y - min_y + 1

    if width >= 5 and height >= 5:
        return (min_x, min_y, width, height, zone_type)
    return None


def generate_road_network(ctx, zones):
    """
    Generate roads through urban zones.
    """
    for zone in zones:
        x, y, w, h, zone_type = zone

        # Create grid of roads through zone
        road_spacing = 12 + ctx.random_int(-2, 2)

        # Horizontal roads
        for road_y in range(y + road_spacing // 2, y + h, road_spacing):
            road_width = 2
            for rx in range(x, x + w):
                for rw in range(road_width):
                    if ctx.in_bounds(rx, road_y + rw):
                        ctx.set_tile(rx, road_y + rw, "asphalt")
                        ctx.mark_occupied(rx, road_y + rw)

        # Vertical roads
        for road_x in range(x + road_spacing // 2, x + w, road_spacing):
            road_width = 2
            for ry in range(y, y + h):
                for rw in range(road_width):
                    if ctx.in_bounds(road_x + rw, ry):
                        ctx.set_tile(road_x + rw, ry, "asphalt")
                        ctx.mark_occupied(road_x + rw, ry)


def place_buildings(ctx, zones):
    """
    Place buildings within urban zones.
    """
    for zone in zones:
        x, y, w, h, zone_type = zone

        # Building parameters based on zone type
        if zone_type == "commercial":
            min_size = 5
            max_size = 12
            wall_tile = "bricks_grey"
            floor_tile = "tiles"
            density = 0.6
        elif zone_type == "industrial":
            min_size = 8
            max_size = 15
            wall_tile = "metal"
            floor_tile = "concrete"
            density = 0.5
        else:  # residential
            min_size = 4
            max_size = 8
            wall_tile = "bricks"
            floor_tile = "wood_floor"
            density = 0.4

        # Try to place buildings
        attempts = (w * h) // 20
        buildings_placed = 0

        for _ in range(attempts):
            if ctx.random() > density:
                continue

            # Random building size and position
            bw = ctx.random_int(min_size, max_size)
            bh = ctx.random_int(min_size, max_size)
            bx = x + ctx.random_int(1, max(1, w - bw - 2))
            by = y + ctx.random_int(1, max(1, h - bh - 2))

            # Check if area is clear
            if is_area_clear(ctx, bx, by, bw, bh):
                # Place building
                place_building(ctx, bx, by, bw, bh, wall_tile, floor_tile)
                buildings_placed += 1


def is_area_clear(ctx, x, y, w, h):
    """
    Check if area is clear for building placement.
    """
    for dy in range(-1, h + 1):
        for dx in range(-1, w + 1):
            px = x + dx
            py = y + dy
            if not ctx.in_bounds(px, py):
                return False
            if ctx.is_occupied(px, py):
                return False
            if ctx.is_water(px, py):
                return False
    return True


def place_building(ctx, x, y, w, h, wall_tile, floor_tile):
    """
    Place a single building.
    """
    wall_height = 2.0 + ctx.random() * 1.5

    # Floor
    for dy in range(1, h - 1):
        for dx in range(1, w - 1):
            ctx.set_tile(x + dx, y + dy, floor_tile)
            ctx.mark_occupied(x + dx, y + dy)

    # Walls - top and bottom
    for dx in range(w):
        ctx.set_wall(x + dx, y, wall_tile, wall_height)
        ctx.mark_occupied(x + dx, y)
        ctx.set_wall(x + dx, y + h - 1, wall_tile, wall_height)
        ctx.mark_occupied(x + dx, y + h - 1)

    # Walls - left and right
    for dy in range(1, h - 1):
        ctx.set_wall(x, y + dy, wall_tile, wall_height)
        ctx.mark_occupied(x, y + dy)
        ctx.set_wall(x + w - 1, y + dy, wall_tile, wall_height)
        ctx.mark_occupied(x + w - 1, y + dy)

    # Entrance (on south side)
    entrance_x = x + w // 2
    entrance_y = y + h - 1
    ctx.set_tile(entrance_x, entrance_y, floor_tile)


def add_urban_details(ctx):
    """
    Add urban details like streetlights, benches, etc.
    """
    for y in range(ctx.height):
        for x in range(ctx.width):
            # Skip if not urban or already occupied
            biome = ctx.get_biome(x, y)
            if biome not in ["urban", "commercial", "residential"]:
                continue
            if ctx.is_occupied(x, y):
                continue

            # Occasional sidewalk tiles near roads
            has_adjacent_road = False
            for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
                nx, ny = x + dx, y + dy
                if ctx.in_bounds(nx, ny) and ctx.is_road(nx, ny):
                    has_adjacent_road = True
                    break

            if has_adjacent_road:
                ctx.set_tile(x, y, "tiles")


def spawn_urban_entities(ctx):
    """
    Spawn NPCs and other entities in urban areas.
    """
    for y in range(0, ctx.height, 5):
        for x in range(0, ctx.width, 5):
            biome = ctx.get_biome(x, y)

            if biome not in ["urban", "commercial", "residential"]:
                continue
            if ctx.is_occupied(x, y):
                continue
            if not ctx.is_walkable(x, y):
                continue

            # Spawn civilians
            if ctx.random() < 0.02:
                ctx.spawn_entity(x, y, "civilian")

            # Spawn enemies (zombies)
            if ctx.random() < 0.01:
                ctx.spawn_entity(x, y, "zombie")
