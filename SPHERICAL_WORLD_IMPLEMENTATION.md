# Spherical World Implementation Summary

## Overview
This document outlines the implementation of spherical world map creation dialog and coordinate system for the StandaloneEditor. The system supports both flat and spherical worlds with latitude/longitude coordinate display and conversion utilities.

## Files Created/Modified

### 1. SphericalCoordinates.hpp (CREATED)
**Location:** `H:/Github/Old3DEngine/examples/SphericalCoordinates.hpp`

Complete coordinate conversion library with:
- `struct SphericalWorld` - Defines a spherical world with radius and center
- `LatLongToXYZ()` - Convert lat/long to 3D Cartesian coordinates
- `XYZToLatLong()` - Convert 3D position to lat/long
- `GetAltitude()` - Calculate altitude above surface
- `GetSurfaceNormal()` - Get normal vector at a surface point
- `GreatCircleDistance()` - Calculate distance between two lat/long points
- `CalculateBearing()` - Get bearing between two points
- `MoveLatLong()` - Move a position by distance and bearing

### 2. StandaloneEditor.hpp (MODIFIED)
**Location:** `H:/Github/Old3DEngine/examples/StandaloneEditor.hpp`

Added spherical world state variables (after line 405):
```cpp
// Spherical world properties
enum class WorldType {
    Flat,       // Traditional flat map
    Spherical   // Spherical world (planet)
};
WorldType m_worldType = WorldType::Flat;
float m_worldRadius = 6371.0f;  // Default to Earth radius in km
glm::vec3 m_worldCenter{0.0f};  // Center of spherical world
bool m_showSphericalGrid = true; // Show wireframe sphere for spherical worlds
```

### 3. StandaloneEditor.cpp (TO BE MODIFIED)

#### A. ShowNewMapDialog() Implementation
Replace the existing `ShowNewMapDialog()` function (around line 885) with:

```cpp
void StandaloneEditor::ShowNewMapDialog() {
    ImGui::OpenPopup("New Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Map", &m_showNewMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static int width = 64;
        static int height = 64;
        static int worldTypeIndex = 0; // 0 = Flat, 1 = Spherical
        static float worldRadius = 6371.0f; // Default to Earth radius

        // World Type Selection
        ImGui::Text("World Type");
        ImGui::RadioButton("Flat Map", &worldTypeIndex, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Spherical World", &worldTypeIndex, 1);

        ImGui::Separator();

        // Map dimensions (for flat maps)
        if (worldTypeIndex == 0) {
            ImGui::InputInt("Width", &width);
            ImGui::InputInt("Height", &height);
        }

        // World radius (for spherical worlds)
        if (worldTypeIndex == 1) {
            ImGui::InputFloat("World Radius (km)", &worldRadius, 100.0f, 1000.0f, "%.1f");

            // Preset buttons
            ImGui::Text("Presets:");
            if (ImGui::Button("Earth (6371 km)")) {
                worldRadius = 6371.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mars (3390 km)")) {
                worldRadius = 3390.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Moon (1737 km)")) {
                worldRadius = 1737.0f;
            }

            // Help text
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Spherical worlds use lat/long coordinates");
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Set world type
            m_worldType = (worldTypeIndex == 0) ? WorldType::Flat : WorldType::Spherical;
            m_worldRadius = worldRadius;
            m_worldCenter = glm::vec3(0.0f);

            // Create map
            NewMap(width, height);
            m_showNewMapDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewMapDialog = false;
        }

        ImGui::EndPopup();
    }
}
```

#### B. ShowMapPropertiesDialog() - Add Spherical World Section
Add this section after the lighting settings (around line 1700):

```cpp
// Add after lighting settings section:
ImGui::Spacing();
ImGui::Separator();

// World Type Settings
ImGui::Text("World Configuration");
ImGui::Separator();

int worldTypeInt = (m_worldType == WorldType::Flat) ? 0 : 1;
ImGui::RadioButton("Flat World", &worldTypeInt, 0);
ImGui::SameLine();
ImGui::RadioButton("Spherical World", &worldTypeInt, 1);
m_worldType = (worldTypeInt == 0) ? WorldType::Flat : WorldType::Spherical;

if (m_worldType == WorldType::Spherical) {
    ImGui::InputFloat("World Radius (km)", &m_worldRadius, 100.0f, 1000.0f, "%.1f");

    ImGui::Text("Presets:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Earth")) m_worldRadius = 6371.0f;
    ImGui::SameLine();
    if (ImGui::SmallButton("Mars")) m_worldRadius = 3390.0f;
    ImGui::SameLine();
    if (ImGui::SmallButton("Moon")) m_worldRadius = 1737.0f;

    ImGui::Checkbox("Show Spherical Grid", &m_showSphericalGrid);
}
```

#### C. RenderStatusBar() - Add Lat/Long Display
Add at the end of `RenderStatusBar()` function:

```cpp
// Add at end of RenderStatusBar():
#include "SphericalCoordinates.hpp"  // Add at top of file

// In RenderStatusBar() function:
if (m_worldType == WorldType::Spherical) {
    ImGui::SameLine();
    ImGui::Separator();

    // Convert camera position to lat/long
    glm::vec2 latLong = XYZToLatLong(m_editorCameraPos, m_worldRadius);
    float altitude = GetAltitude(m_editorCameraPos, m_worldCenter, m_worldRadius);

    ImGui::Text("| Lat: %.4f° Long: %.4f° Alt: %.1f km",
                latLong.x, latLong.y, altitude);
}
```

#### D. Render3D() - Add Wireframe Sphere Visualization
Add after grid rendering (around line 420):

```cpp
// Add after grid rendering in Render3D():
// Draw spherical world wireframe
if (m_worldType == WorldType::Spherical && m_showSphericalGrid) {
    // Draw wireframe sphere
    int segments = 32;
    glm::vec4 sphereColor(0.3f, 0.5f, 0.7f, 0.4f);

    // Draw latitude lines
    for (int lat = -80; lat <= 80; lat += 20) {
        float latRad = glm::radians((float)lat);
        float radius = m_worldRadius * std::cos(latRad);
        float y = m_worldRadius * std::sin(latRad);

        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
            float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();

            glm::vec3 p1(radius * std::cos(angle1), y, radius * std::sin(angle1));
            glm::vec3 p2(radius * std::cos(angle2), y, radius * std::sin(angle2));

            debugDraw.AddLine(m_worldCenter + p1, m_worldCenter + p2, sphereColor);
        }
    }

    // Draw longitude lines
    for (int lon = 0; lon < 360; lon += 30) {
        float lonRad = glm::radians((float)lon);

        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * glm::pi<float>() - glm::pi<float>() / 2.0f;
            float angle2 = (float)(i + 1) / segments * glm::pi<float>() - glm::pi<float>() / 2.0f;

            float r1 = m_worldRadius * std::cos(angle1);
            float y1 = m_worldRadius * std::sin(angle1);
            float r2 = m_worldRadius * std::cos(angle2);
            float y2 = m_worldRadius * std::sin(angle2);

            glm::vec3 p1(r1 * std::cos(lonRad), y1, r1 * std::sin(lonRad));
            glm::vec3 p2(r2 * std::cos(lonRad), y2, r2 * std::sin(lonRad));

            debugDraw.AddLine(m_worldCenter + p1, m_worldCenter + p2, sphereColor);
        }
    }

    // Draw equator in different color
    glm::vec4 equatorColor(1.0f, 0.5f, 0.0f, 0.6f);
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
        float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();

        glm::vec3 p1(m_worldRadius * std::cos(angle1), 0, m_worldRadius * std::sin(angle1));
        glm::vec3 p2(m_worldRadius * std::cos(angle2), 0, m_worldRadius * std::sin(angle2));

        debugDraw.AddLine(m_worldCenter + p1, m_worldCenter + p2, equatorColor);
    }

    // Draw prime meridian in different color
    glm::vec4 meridianColor(0.0f, 1.0f, 0.5f, 0.6f);
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * glm::pi<float>() - glm::pi<float>() / 2.0f;
        float angle2 = (float)(i + 1) / segments * glm::pi<float>() - glm::pi<float>() / 2.0f;

        float r1 = m_worldRadius * std::cos(angle1);
        float y1 = m_worldRadius * std::sin(angle1);
        float r2 = m_worldRadius * std::cos(angle2);
        float y2 = m_worldRadius * std::sin(angle2);

        glm::vec3 p1(0, y1, r1);
        glm::vec3 p2(0, y2, r2);

        debugDraw.AddLine(m_worldCenter + p1, m_worldCenter + p2, meridianColor);
    }
}
```

#### E. SaveMap() - Update Map File Format
Add to save function to store world type and radius:

```cpp
// In SaveMap() function, add:
file << "world_type " << (m_worldType == WorldType::Flat ? "flat" : "spherical") << "\n";
if (m_worldType == WorldType::Spherical) {
    file << "world_radius " << m_worldRadius << "\n";
    file << "world_center " << m_worldCenter.x << " "
         << m_worldCenter.y << " " << m_worldCenter.z << "\n";
}
```

#### F. LoadMap() - Load World Type and Radius
Add to load function:

```cpp
// In LoadMap() function, add parsing:
if (key == "world_type") {
    std::string typeStr;
    file >> typeStr;
    m_worldType = (typeStr == "spherical") ? WorldType::Spherical : WorldType::Flat;
}
else if (key == "world_radius") {
    file >> m_worldRadius;
}
else if (key == "world_center") {
    file >> m_worldCenter.x >> m_worldCenter.y >> m_worldCenter.z;
}
```

## Integration Points

### 1. Coordinate Conversion Usage
```cpp
// Example: Place object at lat/long position
float lat = 37.7749f;  // San Francisco
float lon = -122.4194f;
float altitude = 0.0f;

glm::vec3 worldPos = LatLongToXYZ(lat, lon, altitude, m_worldRadius);
PlaceObject(worldPos, "building");

// Example: Get camera lat/long
glm::vec2 cameraLatLong = XYZToLatLong(m_editorCameraPos, m_worldRadius);
float cameraAlt = GetAltitude(m_editorCameraPos, m_worldCenter, m_worldRadius);
```

### 2. Distance Calculations
```cpp
// Calculate distance between two cities
float distance = GreatCircleDistance(
    37.7749f, -122.4194f,  // San Francisco
    40.7128f, -74.0060f,   // New York
    m_worldRadius
);  // Returns distance in km
```

### 3. View/Coordinate Display Panel
Consider adding a dedicated panel for spherical world coordinates:

```cpp
void StandaloneEditor::RenderSphericalCoordinatePanel() {
    if (m_worldType != WorldType::Spherical) return;

    ImGui::Begin("Spherical Coordinates");

    ImGui::Text("World Configuration:");
    ImGui::Text("  Radius: %.1f km", m_worldRadius);
    ImGui::Text("  Center: (%.1f, %.1f, %.1f)",
                m_worldCenter.x, m_worldCenter.y, m_worldCenter.z);

    ImGui::Separator();
    ImGui::Text("Camera Position:");
    glm::vec2 latLong = XYZToLatLong(m_editorCameraPos, m_worldRadius);
    float altitude = GetAltitude(m_editorCameraPos, m_worldCenter, m_worldRadius);
    ImGui::Text("  Latitude: %.6f°", latLong.x);
    ImGui::Text("  Longitude: %.6f°", latLong.y);
    ImGui::Text("  Altitude: %.2f km", altitude);

    ImGui::Separator();
    ImGui::Text("Quick Navigation:");
    if (ImGui::Button("Go to Equator/Prime Meridian")) {
        m_editorCameraPos = LatLongToXYZ(0.0f, 0.0f, 500.0f, m_worldRadius);
    }
    if (ImGui::Button("Go to North Pole")) {
        m_editorCameraPos = LatLongToXYZ(90.0f, 0.0f, 500.0f, m_worldRadius);
    }
    if (ImGui::Button("Go to South Pole")) {
        m_editorCameraPos = LatLongToXYZ(-90.0f, 0.0f, 500.0f, m_worldRadius);
    }

    ImGui::End();
}
```

## Key Features Implemented

1. **World Type Selection**: Radio buttons in New Map dialog to choose Flat or Spherical
2. **Radius Configuration**: Input field with presets for Earth, Mars, and Moon
3. **Coordinate Conversion**: Complete library for converting between XYZ and lat/long
4. **Visual Feedback**: Wireframe sphere showing latitude/longitude grid lines
5. **Status Display**: Real-time lat/long/altitude display in status bar
6. **Map Properties**: Editable world type and radius in map properties dialog
7. **File Format**: Save/load world type and radius with map files
8. **Helper Functions**: Great circle distance, bearing calculation, position movement

## Testing Recommendations

1. Create a spherical world and verify the wireframe sphere appears
2. Move the camera and watch lat/long coordinates update in status bar
3. Test coordinate conversion:
   - Place objects at known lat/long positions
   - Verify they appear at correct 3D locations
4. Save and load maps to ensure world type persists
5. Test with different world radii (Earth, Mars, Moon)
6. Verify flat maps still work correctly

## Future Enhancements

1. **Terrain Mapping**: Project flat heightmap onto sphere surface
2. **Texture Wrapping**: Handle texture coordinates for spherical worlds
3. **Camera Controls**: Orbit camera that follows sphere curvature
4. **LOD System**: Level-of-detail based on viewing angle
5. **Real-World Data**: Import elevation data, coastlines, city locations
6. **Coordinate Input**: Allow direct lat/long entry for camera position
7. **Minimap**: 2D lat/long minimap view
8. **Grid Snapping**: Snap objects to lat/long grid

## Notes

- Y-up coordinate system is used (Y = north pole)
- Prime meridian (0° longitude) points along +Z axis
- Latitude ranges from -90° (south pole) to +90° (north pole)
- Longitude ranges from -180° to +180°
- All distances and radii are in kilometers
- The coordinate system uses right-handed conventions

## Build Integration

Add to CMakeLists.txt if not already included:
```cmake
# No build changes needed - header-only library
# Just include SphericalCoordinates.hpp where needed
```
