# @name: Road Network Generator
# @version: 1.0.0
# @author: Nova3D Engine Team
# @description: Procedural road network generation with pathfinding

def generate(ctx):
    """
    Generate road network connecting important areas.
    """
    # Phase 1: Identify key locations to connect
    key_locations = find_key_locations(ctx)

    # Phase 2: Create main roads between key locations
    main_roads = generate_main_roads(ctx, key_locations)

    # Phase 3: Create secondary road grid in urban areas
    generate_urban_grid(ctx)

    # Phase 4: Add connecting roads
    connect_road_segments(ctx, main_roads)

    # Phase 5: Add road details
    add_road_details(ctx)


def preview(ctx):
    """
    Fast preview of road network.
    """
    # Just show existing roads from real data
    for y in range(ctx.height):
        for x in range(ctx.width):
            if ctx.is_road(x, y):
                ctx.set_tile(x, y, "asphalt")
                ctx.mark_occupied(x, y)


def find_key_locations(ctx):
    """
    Find important locations that need road connections.
    """
    locations = []

    # Sample the map to find urban centers
    sample_size = 20
    for y in range(sample_size // 2, ctx.height, sample_size):
        for x in range(sample_size // 2, ctx.width, sample_size):
            biome = ctx.get_biome(x, y)
            pop = ctx.get_population_density(x, y)

            # Urban centers
            if biome in ["urban", "commercial"] and pop > 100:
                locations.append({
                    "x": x,
                    "y": y,
                    "type": "urban_center",
                    "priority": 1
                })
            # Residential areas
            elif biome in ["residential", "suburban"]:
                locations.append({
                    "x": x,
                    "y": y,
                    "type": "residential",
                    "priority": 2
                })
            # Industrial zones
            elif biome == "industrial":
                locations.append({
                    "x": x,
                    "y": y,
                    "type": "industrial",
                    "priority": 2
                })

    return locations


def generate_main_roads(ctx, locations):
    """
    Generate main roads connecting key locations.
    """
    roads = []

    if len(locations) < 2:
        return roads

    # Sort by priority
    locations.sort(key=lambda l: l["priority"])

    # Connect each location to its nearest neighbor
    connected = set()

    for i, loc in enumerate(locations):
        if i in connected:
            continue

        # Find nearest unconnected location
        nearest_idx = -1
        nearest_dist = float('inf')

        for j, other in enumerate(locations):
            if i == j or j in connected:
                continue

            dist = ctx.distance(loc["x"], loc["y"], other["x"], other["y"])
            if dist < nearest_dist:
                nearest_dist = dist
                nearest_idx = j

        if nearest_idx >= 0:
            # Create road between locations
            road = create_road(ctx, loc, locations[nearest_idx], "main")
            if road:
                roads.append(road)
                connected.add(i)
                connected.add(nearest_idx)

    return roads


def create_road(ctx, start, end, road_type):
    """
    Create a road between two points using pathfinding.
    """
    # Get road parameters based on type
    if road_type == "main":
        width = 3
        tile = "asphalt"
    elif road_type == "secondary":
        width = 2
        tile = "asphalt"
    else:
        width = 1
        tile = "dirt"

    # Find path (or use straight line with some variation)
    path = find_road_path(ctx, start["x"], start["y"], end["x"], end["y"])

    if not path:
        # Fallback to straight line
        path = bresenham_line(start["x"], start["y"], end["x"], end["y"])

    # Draw road along path
    road_tiles = []
    for point in path:
        x, y = point
        for dx in range(-(width // 2), width // 2 + 1):
            for dy in range(-(width // 2), width // 2 + 1):
                px = x + dx
                py = y + dy
                if ctx.in_bounds(px, py) and not ctx.is_water(px, py):
                    ctx.set_tile(px, py, tile)
                    ctx.mark_occupied(px, py)
                    road_tiles.append((px, py))

    return {
        "start": start,
        "end": end,
        "type": road_type,
        "tiles": road_tiles
    }


def find_road_path(ctx, x1, y1, x2, y2):
    """
    Find path for road using A* with terrain costs.
    """
    # Use context's pathfinding
    path = ctx.find_path(x1, y1, x2, y2)

    # Convert to list of tuples if path exists
    if path:
        return [(p.x, p.y) for p in path]

    return None


def bresenham_line(x1, y1, x2, y2):
    """
    Generate points along a line using Bresenham's algorithm.
    """
    points = []

    dx = abs(x2 - x1)
    dy = abs(y2 - y1)
    sx = 1 if x1 < x2 else -1
    sy = 1 if y1 < y2 else -1
    err = dx - dy

    x, y = x1, y1

    while True:
        points.append((x, y))

        if x == x2 and y == y2:
            break

        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x += sx
        if e2 < dx:
            err += dx
            y += sy

    return points


def generate_urban_grid(ctx):
    """
    Generate grid road pattern in urban areas.
    """
    for y in range(ctx.height):
        for x in range(ctx.width):
            biome = ctx.get_biome(x, y)

            if biome not in ["urban", "commercial", "residential"]:
                continue

            # Create grid every N tiles
            grid_spacing = 15

            is_road_position = (x % grid_spacing < 2) or (y % grid_spacing < 2)

            if is_road_position:
                if not ctx.is_occupied(x, y) and not ctx.is_water(x, y):
                    ctx.set_tile(x, y, "asphalt")
                    ctx.mark_occupied(x, y)


def connect_road_segments(ctx, main_roads):
    """
    Connect isolated road segments.
    """
    # Find road endpoints that aren't connected
    endpoints = []

    for road in main_roads:
        endpoints.append((road["start"]["x"], road["start"]["y"]))
        endpoints.append((road["end"]["x"], road["end"]["y"]))

    # Try to connect nearby endpoints
    for i, ep1 in enumerate(endpoints):
        for j, ep2 in enumerate(endpoints):
            if i >= j:
                continue

            dist = ctx.distance(ep1[0], ep1[1], ep2[0], ep2[1])

            # Connect if close but not too close
            if 5 < dist < 30:
                # Check if already connected via road
                if not has_road_between(ctx, ep1, ep2):
                    create_connecting_road(ctx, ep1, ep2)


def has_road_between(ctx, p1, p2):
    """
    Check if there's a road between two points.
    """
    # Simple line check
    points = bresenham_line(p1[0], p1[1], p2[0], p2[1])
    road_tiles = 0

    for x, y in points:
        if ctx.in_bounds(x, y) and ctx.is_road(x, y):
            road_tiles += 1

    return road_tiles > len(points) * 0.7


def create_connecting_road(ctx, p1, p2):
    """
    Create a connecting road between two points.
    """
    points = bresenham_line(p1[0], p1[1], p2[0], p2[1])

    for x, y in points:
        if ctx.in_bounds(x, y) and not ctx.is_water(x, y):
            ctx.set_tile(x, y, "asphalt")
            ctx.mark_occupied(x, y)


def add_road_details(ctx):
    """
    Add road details like markings and sidewalks.
    """
    for y in range(ctx.height):
        for x in range(ctx.width):
            if not ctx.is_road(x, y):
                continue

            # Add sidewalks adjacent to roads
            for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
                nx, ny = x + dx, y + dy

                if ctx.in_bounds(nx, ny):
                    if not ctx.is_occupied(nx, ny) and not ctx.is_water(nx, ny):
                        biome = ctx.get_biome(nx, ny)
                        if biome in ["urban", "commercial", "residential"]:
                            ctx.set_tile(nx, ny, "tiles")
