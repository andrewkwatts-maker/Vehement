# Editor & PCG System Design

## Overview

The Nova3D RTS Editor supports two distinct map types with a unified PCG (Procedural Content Generation) system:

### 1. World Maps (Global)
- **Purpose**: Create entire planets/continents using lat/long coordinates
- **Scale**: Global (kilometers to meters)
- **Coordinates**: Geographic (latitude/longitude)
- **Use Cases**:
  - MMO persistent worlds
  - Large-scale RTS campaigns
  - Open-world exploration
  - Real-world based maps

### 2. Local Maps (Instances)
- **Purpose**: Small isolated maps for specific scenarios
- **Scale**: Local (100m - 1km)
- **Coordinates**: Local XYZ space
- **Use Cases**:
  - Battle arenas
  - Dungeons/caves
  - Player housing
  - Tutorial maps
  - Custom scenarios

## PCG Node Graph System

### Architecture

Similar to UE5's PCG framework, the system uses a visual node graph where:
- **Nodes** = Operations (noise, math, data queries, filters)
- **Connections** = Data flow between nodes
- **Execution** = Graph is evaluated for each point in space

### Node Categories

#### 1. Input Nodes
- **Position Input**: Provides XYZ world coordinates
- **Lat/Long Input**: Provides geographic coordinates
- **Time Input**: Current time/day/season
- **Random Input**: Deterministic random based on seed + position

#### 2. Noise Nodes
- **Perlin Noise**: Classic smooth noise
  - Inputs: Position, Scale, Octaves, Persistence
  - Output: Float value [0-1]

- **Simplex Noise**: Improved Perlin with less directional artifacts
  - Inputs: Position, Scale
  - Output: Float value [0-1]

- **Voronoi**: Cellular/worley noise
  - Inputs: Position, Scale, Randomness
  - Outputs: Distance, Cell ID

- **Fractal Noise**: Combines multiple octaves
  - Inputs: Position, Scale, Octaves, Lacunarity, Gain
  - Output: Float value [0-1]

- **Curl Noise**: Vector field noise for flow
  - Inputs: Position, Scale
  - Output: Vec3 direction

#### 3. Real-World Data Nodes
- **Elevation Data**: Query real-world elevation
  - Inputs: Lat/Long
  - Outputs: Elevation (meters), Slope (degrees)
  - Data Source: SRTM, ASTER GDEM, or custom DEMs

- **Road Distance**: Distance to nearest road
  - Inputs: Lat/Long
  - Outputs: Distance (meters), Road Type
  - Data Source: OpenStreetMap

- **Building Data**: Building density and proximity
  - Inputs: Lat/Long
  - Outputs: Distance to nearest, Density in radius
  - Data Source: OpenStreetMap, municipal data

- **Biome Data**: Climate and vegetation
  - Inputs: Lat/Long
  - Outputs: Temperature, Rainfall, Foliage Type, Density
  - Data Source: Climate databases, satellite imagery

- **Land Cover**: Terrain type classification
  - Inputs: Lat/Long
  - Outputs: Land type (water, forest, urban, etc.)
  - Data Source: Landsat, Sentinel-2

#### 4. Math Nodes
- **Arithmetic**: Add, Subtract, Multiply, Divide
- **Min/Max**: Choose minimum or maximum value
- **Clamp**: Constrain value to range
- **Lerp**: Linear interpolation
- **Smoothstep**: Smooth interpolation with easing
- **Remap**: Remap value from one range to another
- **Power**: Exponential operations
- **Trigonometry**: Sin, Cos, Tan for wave patterns
- **Abs**: Absolute value

#### 5. Filter Nodes
- **Threshold**: Create binary mask from value
- **Gradient**: Create gradient between two values
- **Distance Field**: Signed distance to shape
- **Blur**: Smooth/blur values
- **Erosion**: Simulate erosion patterns
- **Mask Combine**: AND, OR, NOT operations on masks

#### 6. Terrain Nodes
- **Height Output**: Sets terrain height
- **Splat Map**: Controls terrain texture blending
- **Terrain Layer**: Defines material layers
- **Normal Map**: Generates terrain normals
- **Detail Mask**: Controls detail object density

#### 7. Asset Placement Nodes
- **Scatter**: Random scatter points
  - Inputs: Density, Mask, Scale Variation, Rotation Variation
  - Output: List of transforms

- **Grid Placement**: Regular grid pattern
  - Inputs: Spacing, Offset, Mask
  - Output: List of transforms

- **Spline Placement**: Place along path
  - Inputs: Spline, Spacing, Offset
  - Output: List of transforms

- **Cluster**: Group placements in clusters
  - Inputs: Cluster Count, Cluster Radius, Density
  - Output: List of transforms

- **Foliage Spawner**: Tree/grass placement
  - Inputs: Biome, Density, Slope Limit, Elevation Range
  - Output: Asset list with transforms

- **Building Spawner**: Structure placement
  - Inputs: Density, Road Distance, Terrain Type
  - Output: Building list with transforms

### Example Graphs

#### Example 1: Heightmap from Noise + Real Elevation

```
[Position Input] -> [Perlin Noise] -> [Multiply] -> [Add] -> [Height Output]
                      (Scale: 100)      |           |
                                        |           |
[Lat/Long Input] -> [Elevation Data] ->[Multiply]  |
                                        (Scale: 0.3)|
                                                    |
                      [Constant: Base Height 100] -|
```

**Result**: Terrain that follows real-world elevation but adds procedural variation

#### Example 2: Avoid Roads and Buildings

```
[Lat/Long Input] -> [Road Distance] -> [Threshold] -> [Invert] -> [AND] -> [Tree Scatter]
                                         (> 50m)                    |
[Lat/Long Input] -> [Building Data] -> [Threshold] -> [Invert] ---|
                                         (> 100m)
```

**Result**: Trees only spawn away from roads and buildings

#### Example 3: Biome-Based Vegetation

```
[Lat/Long Input] -> [Biome Data] -> [Foliage Density] -> [Tree Scatter]
                         |                                      |
                         |-> [Temperature] -> [Remap] -> [Asset Type Selector]
                                              (0-15°C = Pine)
                                              (15-25°C = Oak)
                                              (25-40°C = Palm)
```

**Result**: Different tree types based on climate

#### Example 4: Erosion-Based Terrain

```
[Position] -> [Perlin Noise] -> [Height Output] -> [Erosion Filter] -> [Final Height]
                |                                        |
                |-> [Scale] -> [Voronoi] -> [Multiply] -|
                                             (Rocky areas)
```

**Result**: Terrain with erosion patterns creating valleys and ridges

## World Map Editor Features

### Navigation
- **Lat/Long Input**: Jump to specific coordinates
- **Search**: Find locations by name (cities, landmarks)
- **Bookmarks**: Save favorite locations
- **Mini-map**: Global overview with current view indicator

### Data Sources Panel
- **Elevation Data**: Configure DEM source and resolution
- **Vector Data**: Configure OSM or custom road/building data
- **Raster Data**: Satellite imagery, land cover
- **Custom Layers**: Import your own geospatial data

### Generation Settings
- **Use Real Elevation**: Toggle real-world vs procedural
- **Blend Amount**: Mix real + procedural (0-100%)
- **Avoid Roads**: Don't place objects on roads
- **Avoid Buildings**: Don't place objects in urban areas
- **Respect Biomes**: Use real climate data for vegetation

### Streaming/LOD
- **Chunk Size**: Size of loaded chunks (e.g., 1km²)
- **LOD Levels**: Number of LOD levels
- **View Distance**: How far to load terrain
- **Generation on Demand**: Generate chunks as player approaches

## Local Map Editor Features

### Map Types
- **Arena**: Flat or hilly terrain for combat
- **Dungeon**: Cave-like enclosed spaces
- **Housing**: Player housing plot
- **Scenario**: Custom mission map
- **Tutorial**: Guided tutorial space

### Inherit from World
- **Source Location**: Pick lat/long from world map
- **Inherit Biome**: Use world biome for theme
- **Inherit Elevation**: Match terrain style
- **Instance Size**: Carve out section (100m - 1km)

### Spawn Points
- **Player Spawns**: Where players start
- **Enemy Spawns**: Where enemies appear
- **Boss Spawns**: Special enemy locations
- **NPC Spawns**: Friendly/neutral characters

### Objectives
- **Capture Points**: Territory to capture
- **Defend Points**: Areas to protect
- **Escort Points**: Waypoints for escort missions
- **Collection Points**: Resource gathering locations

## File Formats

### World Map (.worldmap)
```json
{
  "version": "1.0",
  "bounds": {
    "minLat": -90.0,
    "maxLat": 90.0,
    "minLon": -180.0,
    "maxLon": 180.0
  },
  "resolution": 100,
  "pcgGraph": "path/to/graph.pcg",
  "seed": 12345,
  "dataSources": {
    "elevation": "path/to/dem.tif",
    "roads": "path/to/roads.osm",
    "buildings": "path/to/buildings.osm"
  },
  "chunks": [
    {
      "lat": 40.7128,
      "lon": -74.0060,
      "generated": true,
      "file": "chunks/40_-74.chunk"
    }
  ]
}
```

### Local Map (.localmap)
```json
{
  "version": "1.0",
  "type": "arena",
  "size": {"width": 256, "height": 256},
  "tileSize": 1.0,
  "pcgGraph": "path/to/graph.pcg",
  "seed": 54321,
  "inheritFromWorld": false,
  "heightmap": "path/to/heightmap.raw",
  "splatmap": "path/to/splatmap.png",
  "assets": [
    {
      "type": "tree_oak",
      "position": [10.5, 2.3, 15.7],
      "rotation": [0, 45, 0],
      "scale": [1.2, 1.2, 1.2]
    }
  ],
  "spawnPoints": [
    {
      "type": "player",
      "position": [10, 0, 10],
      "faction": "team1"
    }
  ],
  "objectives": [
    {
      "type": "capture",
      "position": [128, 0, 128],
      "description": "Capture the center"
    }
  ]
}
```

### PCG Graph (.pcg)
```json
{
  "version": "1.0",
  "nodes": [
    {
      "id": 1,
      "type": "PositionInput",
      "position": [100, 100],
      "outputs": [{"pin": 0, "connections": [{"node": 2, "pin": 0}]}]
    },
    {
      "id": 2,
      "type": "PerlinNoise",
      "position": [300, 100],
      "params": {"scale": 50.0, "octaves": 4, "persistence": 0.5},
      "inputs": [{"pin": 0, "connection": {"node": 1, "pin": 0}}],
      "outputs": [{"pin": 0, "connections": [{"node": 3, "pin": 0}]}]
    },
    {
      "id": 3,
      "type": "HeightOutput",
      "position": [500, 100],
      "inputs": [{"pin": 0, "connection": {"node": 2, "pin": 0}}]
    }
  ]
}
```

## Implementation Status

### ✅ Created
- PCGNodeGraph.hpp - Node graph system architecture
- PCGGraphEditor.hpp - Visual node editor interface
- WorldMapEditor.hpp - Global map editor interface
- LocalMapEditor.hpp - Local map editor interface
- Updated StandaloneEditor.hpp - Integration point

### ⏳ To Implement
1. **PCG Node Implementations** (PCGNodeGraph.cpp)
   - Actual noise algorithms (use FastNoise2 library)
   - Math operations
   - Real-world data loaders

2. **PCG Graph Editor** (PCGGraphEditor.cpp)
   - ImGui node editor rendering
   - Connection drawing
   - Drag & drop
   - Property editing

3. **World Map Editor** (WorldMapEditor.cpp)
   - Lat/long navigation
   - Chunk streaming
   - Real-world data loading
   - PCG integration

4. **Local Map Editor** (LocalMapEditor.cpp)
   - Terrain editing
   - Asset placement
   - Spawn point management
   - Objective system

5. **Data Loaders**
   - GeoTIFF reader for elevation data
   - OSM parser for roads/buildings
   - Biome database

6. **Integration**
   - Update StandaloneEditor.cpp to switch between editor types
   - Add menu options for editor type selection
   - File format serialization/deserialization

## Next Steps

1. **Implement core PCG nodes** using FastNoise2
2. **Create basic PCG graph editor UI** with ImGui
3. **Implement local map editor** (simpler, no real-world data needed)
4. **Test PCG system** with simple graphs (noise -> height)
5. **Add real-world data loaders** (optional, for world maps)
6. **Implement world map editor** with chunking/streaming
7. **Polish UI** and add more node types
8. **Add presets** for common scenarios (mountains, plains, desert, etc.)

## Benefits

### For Players
- **Infinite Variety**: Procedural generation creates unique maps every time
- **Real-World Familiarity**: Maps based on real locations feel authentic
- **Customization**: Visual scripting allows non-programmers to create content
- **Performance**: Generate on-demand, only what's needed

### For Developers
- **No Manual Work**: Don't hand-paint massive worlds
- **Rapid Iteration**: Tweak graphs, see results instantly
- **Data-Driven**: Real-world data makes believable environments
- **Scalable**: Works from small arenas to entire planets
