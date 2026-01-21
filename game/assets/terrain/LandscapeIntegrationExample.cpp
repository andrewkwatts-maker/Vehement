/**
 * @file LandscapeIntegrationExample.cpp
 * @brief Example integration of the cinematic landscape system
 *
 * This file demonstrates how to integrate the menu_landscape.json configuration
 * into the RTS application for rendering the main menu background.
 */

#include "engine/procedural/ProcGenGraph.hpp"
#include "engine/procedural/ProcGenNodes.hpp"
#include "engine/terrain/SDFTerrain.hpp"
#include "engine/scripting/visual/VisualScriptingCore.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Mesh.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <any>

namespace Nova {

/**
 * @brief Manages the procedurally generated cinematic landscape
 */
class MenuLandscape {
public:
    MenuLandscape() = default;
    ~MenuLandscape() = default;

    /**
     * @brief Load and generate landscape from configuration file
     */
    bool Initialize(const std::string& configPath) {
        // Load JSON configuration
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            spdlog::error("Failed to open landscape config: {}", configPath);
            return false;
        }

        nlohmann::json config;
        configFile >> config;

        spdlog::info("Loading cinematic landscape: {}", config["name"].get<std::string>());

        // Extract world parameters
        m_worldSize = config["world"]["size"].get<float>();
        m_resolution = config["world"]["resolution"].get<int>();
        m_heightScale = config["world"]["heightScale"].get<float>();
        m_seed = config["world"]["seed"].get<int>();

        // Initialize procedural generation
        if (!InitializeProcGen(config)) {
            return false;
        }

        // Generate terrain heightmap
        if (!GenerateTerrain(config)) {
            return false;
        }

        // Setup SDF terrain for rendering
        if (!SetupSDFTerrain(config)) {
            return false;
        }

        // Place features (rocks, vegetation, water)
        PlaceFeatures(config);

        // Create rendering resources
        CreateRenderingResources(config);

        spdlog::info("Landscape generation complete!");
        return true;
    }

    /**
     * @brief Update landscape (for animated elements if needed)
     */
    void Update(float deltaTime) {
        // Future: Update animated elements like water, wind sway, etc.
    }

    /**
     * @brief Render the landscape
     */
    void Render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                const glm::vec3& cameraPos) {
        if (!m_terrainShader || !m_sdfTerrain) return;

        m_terrainShader->Bind();

        // Set matrices
        m_terrainShader->SetMat4("u_View", viewMatrix);
        m_terrainShader->SetMat4("u_Projection", projMatrix);
        m_terrainShader->SetVec3("u_CameraPos", cameraPos);

        // Set lighting from config
        m_terrainShader->SetVec3("u_LightDirection", m_lightDirection);
        m_terrainShader->SetVec3("u_LightColor", m_lightColor);
        m_terrainShader->SetFloat("u_AmbientStrength", m_ambientStrength);
        m_terrainShader->SetVec3("u_AmbientColor", m_ambientColor);

        // Set atmospheric parameters
        m_terrainShader->SetVec3("u_FogColor", m_fogColor);
        m_terrainShader->SetFloat("u_FogDensity", m_fogDensity);
        m_terrainShader->SetFloat("u_DesaturationAmount", m_desaturationAmount);

        // Bind SDF terrain textures
        m_sdfTerrain->BindForRendering(m_terrainShader.get());

        // Render terrain mesh
        if (m_terrainMesh) {
            m_terrainMesh->Draw();
        }

        // Render features
        RenderFeatures();
    }

    /**
     * @brief Get terrain height at XZ position
     */
    float GetHeightAt(float x, float z) const {
        if (!m_sdfTerrain) return 0.0f;
        return m_sdfTerrain->GetHeightAt(x, z);
    }

private:
    /**
     * @brief Initialize procedural generation system
     */
    bool InitializeProcGen(const nlohmann::json& config) {
        m_procGenGraph = std::make_shared<ProcGen::ProcGenGraph>();

        ProcGen::ProcGenConfig genConfig;
        genConfig.seed = m_seed;
        genConfig.chunkSize = m_resolution;
        genConfig.worldScale = m_worldSize;
        m_procGenGraph->SetConfig(genConfig);

        // Load visual scripting graph if present
        if (config.contains("visual_scripting_graph")) {
            if (!m_procGenGraph->LoadFromJson(config["visual_scripting_graph"])) {
                spdlog::error("Failed to load visual scripting graph");
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Generate terrain using procedural nodes
     */
    bool GenerateTerrain(const nlohmann::json& config) {
        auto terrainGen = config["terrain_generation"];

        // Create heightmap
        m_heightmap = std::make_shared<ProcGen::HeightmapData>(m_resolution, m_resolution);

        spdlog::info("Generating base terrain layers...");

        // Layer 1: Base terrain (rolling hills)
        auto baseTerrain = GenerateBaseLayer(terrainGen["base_terrain"]);

        // Layer 2: Mountains
        auto mountainLayer = GenerateMountainLayer(terrainGen["mountain_layer"]);

        // Layer 3: Detail
        auto detailLayer = GenerateDetailLayer(terrainGen["detail_layer"]);

        // Layer 4: Valley carving
        auto valleyLayer = GenerateValleyLayer(terrainGen["valley_carving"]);

        // Combine layers
        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                float worldX = (x / float(m_resolution) - 0.5f) * m_worldSize;
                float worldZ = (y / float(m_resolution) - 0.5f) * m_worldSize;

                float height = baseTerrain->Get(x, y) +
                               mountainLayer->Get(x, y) +
                               detailLayer->Get(x, y) +
                               valleyLayer->Get(x, y);

                m_heightmap->Set(x, y, height);
            }
        }

        spdlog::info("Applying erosion simulation...");

        // Apply erosion
        if (terrainGen.contains("erosion")) {
            ApplyErosion(terrainGen["erosion"]);
        }

        // Carve hero platform
        if (terrainGen.contains("hero_platform_carving")) {
            CarveHeroPlatform(config["hero_platform"], terrainGen["hero_platform_carving"]);
        }

        return true;
    }

    /**
     * @brief Generate base terrain layer (rolling hills)
     */
    std::shared_ptr<ProcGen::HeightmapData> GenerateBaseLayer(const nlohmann::json& params) {
        auto layer = std::make_shared<ProcGen::HeightmapData>(m_resolution, m_resolution);

        float frequency = params["frequency"].get<float>();
        int octaves = params["octaves"].get<int>();
        float persistence = params["persistence"].get<float>();
        float lacunarity = params["lacunarity"].get<float>();
        float amplitude = params["amplitude"].get<float>();

        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                glm::vec2 pos(x, y);

                // FBM Perlin noise
                float noise = 0.0f;
                float amp = 1.0f;
                float freq = frequency;
                float maxValue = 0.0f;

                for (int i = 0; i < octaves; i++) {
                    glm::vec2 p = pos * freq;
                    noise += (glm::perlin(p) * 0.5f + 0.5f) * amp;
                    maxValue += amp;
                    amp *= persistence;
                    freq *= lacunarity;
                }

                noise /= maxValue;
                layer->Set(x, y, noise * amplitude);
            }
        }

        return layer;
    }

    /**
     * @brief Generate mountain layer (distant peaks)
     */
    std::shared_ptr<ProcGen::HeightmapData> GenerateMountainLayer(const nlohmann::json& params) {
        auto layer = std::make_shared<ProcGen::HeightmapData>(m_resolution, m_resolution);

        float frequency = params["frequency"].get<float>();
        int octaves = params["octaves"].get<int>();
        float persistence = params["persistence"].get<float>();
        float lacunarity = params["lacunarity"].get<float>();
        float amplitude = params["amplitude"].get<float>();
        float sharpness = params["sharpness"].get<float>();

        // Distance falloff
        auto falloff = params["distance_falloff"];
        glm::vec2 center(falloff["center"][0].get<float>(), falloff["center"][1].get<float>());
        float maxDist = falloff["max_distance"].get<float>();
        float falloffPower = falloff["falloff_power"].get<float>();

        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                float worldX = (x / float(m_resolution) - 0.5f) * m_worldSize;
                float worldZ = (y / float(m_resolution) - 0.5f) * m_worldSize;
                glm::vec2 worldPos(worldX, worldZ);

                // Ridge noise (inverted absolute Perlin)
                glm::vec2 pos(x, y);
                float noise = 0.0f;
                float amp = 1.0f;
                float freq = frequency;
                float maxValue = 0.0f;

                for (int i = 0; i < octaves; i++) {
                    glm::vec2 p = pos * freq;
                    float ridge = 1.0f - glm::abs(glm::perlin(p));
                    ridge = glm::pow(ridge, sharpness);
                    noise += ridge * amp;
                    maxValue += amp;
                    amp *= persistence;
                    freq *= lacunarity;
                }

                noise /= maxValue;

                // Apply distance falloff
                float dist = glm::distance(worldPos, center);
                float falloffFactor = glm::clamp(1.0f - glm::pow(dist / maxDist, falloffPower), 0.0f, 1.0f);

                layer->Set(x, y, noise * amplitude * falloffFactor);
            }
        }

        return layer;
    }

    /**
     * @brief Generate detail layer (fine variations)
     */
    std::shared_ptr<ProcGen::HeightmapData> GenerateDetailLayer(const nlohmann::json& params) {
        auto layer = std::make_shared<ProcGen::HeightmapData>(m_resolution, m_resolution);

        float frequency = params["frequency"].get<float>();
        int octaves = params["octaves"].get<int>();
        float persistence = params["persistence"].get<float>();
        float lacunarity = params["lacunarity"].get<float>();
        float amplitude = params["amplitude"].get<float>();

        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                glm::vec2 pos(x, y);

                // FBM Simplex noise
                float noise = 0.0f;
                float amp = 1.0f;
                float freq = frequency;
                float maxValue = 0.0f;

                for (int i = 0; i < octaves; i++) {
                    glm::vec2 p = pos * freq;
                    noise += (glm::simplex(p) * 0.5f + 0.5f) * amp;
                    maxValue += amp;
                    amp *= persistence;
                    freq *= lacunarity;
                }

                noise /= maxValue;
                layer->Set(x, y, noise * amplitude);
            }
        }

        return layer;
    }

    /**
     * @brief Generate valley carving layer
     */
    std::shared_ptr<ProcGen::HeightmapData> GenerateValleyLayer(const nlohmann::json& params) {
        auto layer = std::make_shared<ProcGen::HeightmapData>(m_resolution, m_resolution);

        float scale = params["scale"].get<float>();
        float amplitude = params["amplitude"].get<float>();

        // Simple voronoi-based valleys
        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                glm::vec2 pos(x * scale, y * scale);
                glm::ivec2 cell = glm::ivec2(glm::floor(pos));

                float minDist = 1000.0f;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        glm::ivec2 neighbor = cell + glm::ivec2(dx, dy);
                        // Simple hash for cell point
                        float px = float(neighbor.x) + glm::fract(glm::sin(float(neighbor.x * 127 + neighbor.y * 311)) * 43758.5453f);
                        float py = float(neighbor.y) + glm::fract(glm::sin(float(neighbor.x * 269 + neighbor.y * 183)) * 43758.5453f);
                        glm::vec2 cellPoint(px, py);
                        float dist = glm::distance(pos, cellPoint);
                        minDist = glm::min(minDist, dist);
                    }
                }

                // Valleys are lower areas
                layer->Set(x, y, minDist * amplitude);
            }
        }

        return layer;
    }

    /**
     * @brief Apply erosion simulation
     */
    void ApplyErosion(const nlohmann::json& erosionConfig) {
        // Hydraulic erosion would be applied here using HydraulicErosionNode
        // For brevity, this is a placeholder
        // In production, you'd use the actual node execution

        if (erosionConfig.contains("hydraulic") && erosionConfig["hydraulic"]["enabled"].get<bool>()) {
            spdlog::info("Applying hydraulic erosion...");

            // Create and configure hydraulic erosion node
            auto hydraulicNode = std::make_shared<ProcGen::HydraulicErosionNode>();

            // Set input heightmap
            auto heightmapPort = hydraulicNode->GetInputPort("heightmap");
            if (heightmapPort) {
                heightmapPort->SetValue(std::any(m_heightmap));
            }

            // Configure erosion parameters from JSON
            auto hydraulicParams = erosionConfig["hydraulic"];
            if (auto port = hydraulicNode->GetInputPort("iterations")) {
                port->SetValue(std::any(hydraulicParams.value("iterations", 50000)));
            }
            if (auto port = hydraulicNode->GetInputPort("rainAmount")) {
                port->SetValue(std::any(hydraulicParams.value("rain_amount", 0.01f)));
            }
            if (auto port = hydraulicNode->GetInputPort("evaporation")) {
                port->SetValue(std::any(hydraulicParams.value("evaporation", 0.02f)));
            }
            if (auto port = hydraulicNode->GetInputPort("sedimentCapacity")) {
                port->SetValue(std::any(hydraulicParams.value("sediment_capacity", 4.0f)));
            }
            if (auto port = hydraulicNode->GetInputPort("erosionStrength")) {
                port->SetValue(std::any(hydraulicParams.value("erosion_strength", 0.3f)));
            }
            if (auto port = hydraulicNode->GetInputPort("depositionStrength")) {
                port->SetValue(std::any(hydraulicParams.value("deposition_strength", 0.3f)));
            }

            // Execute erosion
            VisualScript::ExecutionContext context;
            hydraulicNode->Execute(context);

            // Retrieve eroded heightmap
            if (auto outputPort = hydraulicNode->GetOutputPort("erodedHeightmap")) {
                try {
                    m_heightmap = std::any_cast<std::shared_ptr<ProcGen::HeightmapData>>(outputPort->GetValue());
                } catch (const std::bad_any_cast&) {
                    spdlog::warn("Failed to retrieve eroded heightmap from hydraulic erosion node");
                }
            }
        }

        if (erosionConfig.contains("thermal") && erosionConfig["thermal"]["enabled"].get<bool>()) {
            spdlog::info("Applying thermal erosion...");

            // Create and configure thermal erosion node
            auto thermalNode = std::make_shared<ProcGen::ThermalErosionNode>();

            // Set input heightmap
            auto heightmapPort = thermalNode->GetInputPort("heightmap");
            if (heightmapPort) {
                heightmapPort->SetValue(std::any(m_heightmap));
            }

            // Configure erosion parameters from JSON
            auto thermalParams = erosionConfig["thermal"];
            if (auto port = thermalNode->GetInputPort("iterations")) {
                port->SetValue(std::any(thermalParams.value("iterations", 100)));
            }
            if (auto port = thermalNode->GetInputPort("talusAngle")) {
                port->SetValue(std::any(thermalParams.value("talus_angle", 0.6f)));
            }
            if (auto port = thermalNode->GetInputPort("strength")) {
                port->SetValue(std::any(thermalParams.value("strength", 0.4f)));
            }

            // Execute erosion
            VisualScript::ExecutionContext context;
            thermalNode->Execute(context);

            // Retrieve eroded heightmap
            if (auto outputPort = thermalNode->GetOutputPort("erodedHeightmap")) {
                try {
                    m_heightmap = std::any_cast<std::shared_ptr<ProcGen::HeightmapData>>(outputPort->GetValue());
                } catch (const std::bad_any_cast&) {
                    spdlog::warn("Failed to retrieve eroded heightmap from thermal erosion node");
                }
            }
        }
    }

    /**
     * @brief Carve flat platform for hero
     */
    void CarveHeroPlatform(const nlohmann::json& platform, const nlohmann::json& carving) {
        glm::vec2 center(platform["position"][0].get<float>(), platform["position"][2].get<float>());
        float innerRadius = carving["inner_radius"].get<float>();
        float outerRadius = carving["outer_radius"].get<float>();
        float targetHeight = carving["target_height"].get<float>();

        for (int y = 0; y < m_resolution; y++) {
            for (int x = 0; x < m_resolution; x++) {
                float worldX = (x / float(m_resolution) - 0.5f) * m_worldSize;
                float worldZ = (y / float(m_resolution) - 0.5f) * m_worldSize;
                glm::vec2 pos(worldX, worldZ);

                float dist = glm::distance(pos, center);

                if (dist < innerRadius) {
                    // Fully flat
                    m_heightmap->Set(x, y, targetHeight);
                } else if (dist < outerRadius) {
                    // Blend zone
                    float t = (dist - innerRadius) / (outerRadius - innerRadius);
                    // Smoothstep
                    t = t * t * (3.0f - 2.0f * t);
                    float currentHeight = m_heightmap->Get(x, y);
                    m_heightmap->Set(x, y, glm::mix(targetHeight, currentHeight, t));
                }
            }
        }
    }

    /**
     * @brief Setup SDF terrain for rendering
     */
    bool SetupSDFTerrain(const nlohmann::json& config) {
        m_sdfTerrain = std::make_shared<SDFTerrain>();

        SDFTerrain::Config sdfConfig;
        auto perfConfig = config["performance"]["sdf_terrain"];

        sdfConfig.resolution = perfConfig["resolution"].get<int>();
        sdfConfig.worldSize = perfConfig["world_size"].get<float>();
        sdfConfig.maxHeight = perfConfig["max_height"].get<float>();
        sdfConfig.octreeLevels = perfConfig["octree_levels"].get<int>();
        sdfConfig.useOctree = perfConfig["use_octree"].get<bool>();
        sdfConfig.sparseStorage = perfConfig["sparse_storage"].get<bool>();

        if (!m_sdfTerrain->Initialize(sdfConfig)) {
            spdlog::error("Failed to initialize SDF terrain");
            return false;
        }

        // Build SDF from heightmap
        std::vector<float> heightData = m_heightmap->GetData();
        m_sdfTerrain->BuildFromHeightmap(heightData.data(), m_resolution, m_resolution);

        // Upload to GPU
        m_sdfTerrain->UploadToGPU();

        return true;
    }

    /**
     * @brief Place features (rocks, vegetation, water)
     */
    void PlaceFeatures(const nlohmann::json& config) {
        auto features = config["features"];

        // Place rocks
        if (features.contains("rocks") && features["rocks"]["enabled"].get<bool>()) {
            PlaceRocks(features["rocks"]);
        }

        // Place vegetation
        if (features.contains("vegetation") && features["vegetation"]["enabled"].get<bool>()) {
            PlaceVegetation(features["vegetation"]);
        }

        // Place water
        if (features.contains("water") && features["water"]["enabled"].get<bool>()) {
            PlaceWater(features["water"]);
        }
    }

    void PlaceRocks(const nlohmann::json& rockConfig) {
        // Implementation would use ResourcePlacementNode
        spdlog::info("Placing rocks...");
    }

    void PlaceVegetation(const nlohmann::json& vegConfig) {
        // Implementation would use VegetationPlacementNode
        spdlog::info("Placing vegetation...");
    }

    void PlaceWater(const nlohmann::json& waterConfig) {
        // Implementation would create water bodies
        spdlog::info("Placing water features...");
    }

    /**
     * @brief Create rendering resources (shaders, meshes)
     */
    void CreateRenderingResources(const nlohmann::json& config) {
        // Load terrain shader from files
        m_terrainShader = std::make_shared<Shader>();
        const std::string shaderBasePath = "game/assets/shaders/terrain/";
        if (!m_terrainShader->Load(shaderBasePath + "terrain.vert", shaderBasePath + "terrain.frag")) {
            spdlog::warn("Failed to load terrain shader from files, using fallback embedded shader");

            // Fallback: use embedded simple terrain shader
            const char* vertexSource = R"(
                #version 450 core
                layout(location = 0) in vec3 a_Position;
                layout(location = 1) in vec3 a_Normal;
                layout(location = 2) in vec2 a_TexCoord;

                uniform mat4 u_View;
                uniform mat4 u_Projection;

                out vec3 v_WorldPos;
                out vec3 v_Normal;
                out vec2 v_TexCoord;

                void main() {
                    v_WorldPos = a_Position;
                    v_Normal = a_Normal;
                    v_TexCoord = a_TexCoord;
                    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
                }
            )";

            const char* fragmentSource = R"(
                #version 450 core
                in vec3 v_WorldPos;
                in vec3 v_Normal;
                in vec2 v_TexCoord;

                uniform vec3 u_CameraPos;
                uniform vec3 u_LightDirection;
                uniform vec3 u_LightColor;
                uniform float u_AmbientStrength;
                uniform vec3 u_AmbientColor;
                uniform vec3 u_FogColor;
                uniform float u_FogDensity;
                uniform float u_DesaturationAmount;

                out vec4 FragColor;

                void main() {
                    // Basic terrain coloring based on height and slope
                    vec3 normal = normalize(v_Normal);
                    float slope = 1.0 - normal.y;
                    float height = v_WorldPos.y;

                    // Terrain color gradient
                    vec3 grassColor = vec3(0.3, 0.5, 0.2);
                    vec3 rockColor = vec3(0.5, 0.45, 0.4);
                    vec3 snowColor = vec3(0.95, 0.95, 0.98);

                    vec3 baseColor = mix(grassColor, rockColor, smoothstep(0.3, 0.7, slope));
                    baseColor = mix(baseColor, snowColor, smoothstep(40.0, 50.0, height));

                    // Lighting
                    float NdotL = max(dot(normal, normalize(-u_LightDirection)), 0.0);
                    vec3 diffuse = NdotL * u_LightColor;
                    vec3 ambient = u_AmbientStrength * u_AmbientColor;

                    vec3 color = baseColor * (ambient + diffuse);

                    // Atmospheric fog
                    float dist = length(v_WorldPos - u_CameraPos);
                    float fogFactor = 1.0 - exp(-u_FogDensity * dist * 0.001);
                    color = mix(color, u_FogColor, fogFactor);

                    // Distance desaturation
                    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
                    color = mix(color, vec3(luminance), fogFactor * u_DesaturationAmount);

                    FragColor = vec4(color, 1.0);
                }
            )";

            if (!m_terrainShader->LoadFromSource(vertexSource, fragmentSource)) {
                spdlog::error("Failed to compile fallback terrain shader");
            }
        }

        // Create terrain mesh from heightmap
        m_terrainMesh = CreateTerrainMesh();

        // Extract lighting parameters
        auto lighting = config["lighting"];
        auto primaryLight = lighting["primary_light"];
        m_lightDirection = glm::vec3(
            primaryLight["direction"][0].get<float>(),
            primaryLight["direction"][1].get<float>(),
            primaryLight["direction"][2].get<float>()
        );
        m_lightColor = glm::vec3(
            primaryLight["color"][0].get<float>(),
            primaryLight["color"][1].get<float>(),
            primaryLight["color"][2].get<float>()
        );

        auto ambient = lighting["ambient"];
        m_ambientStrength = ambient["intensity"].get<float>();
        m_ambientColor = glm::vec3(
            ambient["color"][0].get<float>(),
            ambient["color"][1].get<float>(),
            ambient["color"][2].get<float>()
        );

        // Extract atmospheric parameters
        auto atmosphere = config["atmospheric_perspective"];
        auto fog = atmosphere["distance_fog"];
        m_fogColor = glm::vec3(
            fog["color"][0].get<float>(),
            fog["color"][1].get<float>(),
            fog["color"][2].get<float>()
        );
        m_fogDensity = fog["density"].get<float>();

        auto colorGrading = atmosphere["color_grading"];
        m_desaturationAmount = colorGrading["distance_desaturation"]["amount"].get<float>();
    }

    /**
     * @brief Create terrain mesh from heightmap
     */
    std::shared_ptr<Mesh> CreateTerrainMesh() {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        int gridSize = m_resolution - 1;
        float step = m_worldSize / gridSize;

        // Generate vertices
        for (int z = 0; z < m_resolution; z++) {
            for (int x = 0; x < m_resolution; x++) {
                float worldX = (x / float(gridSize) - 0.5f) * m_worldSize;
                float worldZ = (z / float(gridSize) - 0.5f) * m_worldSize;
                float height = m_heightmap->Get(x, z);

                // Position
                vertices.push_back(worldX);
                vertices.push_back(height);
                vertices.push_back(worldZ);

                // Normal (calculated from heightmap)
                glm::vec3 normal = m_heightmap->GetNormal(x, z, step);
                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);

                // UV
                vertices.push_back(x / float(gridSize));
                vertices.push_back(z / float(gridSize));
            }
        }

        // Generate indices (triangle strip or indexed triangles)
        for (int z = 0; z < gridSize; z++) {
            for (int x = 0; x < gridSize; x++) {
                int topLeft = z * m_resolution + x;
                int topRight = topLeft + 1;
                int bottomLeft = (z + 1) * m_resolution + x;
                int bottomRight = bottomLeft + 1;

                // First triangle
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                // Second triangle
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        // Create mesh and upload to GPU
        auto mesh = std::make_shared<Mesh>();

        // Calculate vertex count and index count
        size_t vertexCount = static_cast<size_t>(m_resolution) * m_resolution;
        size_t indexCount = indices.size();

        // Upload vertices and indices to GPU using CreateFromRaw
        // Vertex format: position (3 floats) + normal (3 floats) + texcoord (2 floats) = 8 floats per vertex
        mesh->CreateFromRaw(
            vertices.data(),
            vertexCount,
            indices.data(),
            indexCount,
            true,   // hasNormals
            true,   // hasTexCoords
            false   // hasTangents
        );

        spdlog::info("Created terrain mesh with {} vertices and {} indices", vertexCount, indexCount);
        return mesh;
    }

    void RenderFeatures() {
        // Render rocks using instanced rendering
        if (m_rockShader && m_rockMesh && !m_rockTransforms.empty()) {
            m_rockShader->Bind();
            m_rockShader->SetVec3("u_LightDirection", m_lightDirection);
            m_rockShader->SetVec3("u_LightColor", m_lightColor);
            m_rockShader->SetFloat("u_AmbientStrength", m_ambientStrength);
            m_rockShader->SetVec3("u_AmbientColor", m_ambientColor);
            m_rockShader->SetVec3("u_FogColor", m_fogColor);
            m_rockShader->SetFloat("u_FogDensity", m_fogDensity);

            // Render each rock instance
            for (const auto& transform : m_rockTransforms) {
                m_rockShader->SetMat4("u_Model", transform);
                m_rockMesh->Draw();
            }
        }

        // Render vegetation (grass, trees, bushes)
        if (m_vegetationShader && !m_vegetationInstances.empty()) {
            m_vegetationShader->Bind();
            m_vegetationShader->SetVec3("u_LightDirection", m_lightDirection);
            m_vegetationShader->SetVec3("u_LightColor", m_lightColor);
            m_vegetationShader->SetFloat("u_AmbientStrength", m_ambientStrength);
            m_vegetationShader->SetVec3("u_AmbientColor", m_ambientColor);
            m_vegetationShader->SetVec3("u_FogColor", m_fogColor);
            m_vegetationShader->SetFloat("u_FogDensity", m_fogDensity);

            // Render vegetation by type
            for (const auto& [mesh, transforms] : m_vegetationInstances) {
                if (mesh && !transforms.empty()) {
                    for (const auto& transform : transforms) {
                        m_vegetationShader->SetMat4("u_Model", transform);
                        mesh->Draw();
                    }
                }
            }
        }

        // Render water plane with special water shader
        if (m_waterShader && m_waterMesh) {
            m_waterShader->Bind();
            m_waterShader->SetFloat("u_WaterLevel", m_waterLevel);
            m_waterShader->SetVec3("u_WaterColor", m_waterColor);
            m_waterShader->SetFloat("u_WaterOpacity", m_waterOpacity);
            m_waterShader->SetVec3("u_LightDirection", m_lightDirection);
            m_waterShader->SetVec3("u_LightColor", m_lightColor);
            m_waterShader->SetVec3("u_FogColor", m_fogColor);
            m_waterShader->SetFloat("u_FogDensity", m_fogDensity);
            m_waterMesh->Draw();
        }
    }

private:
    // Procedural generation
    std::shared_ptr<ProcGen::ProcGenGraph> m_procGenGraph;
    std::shared_ptr<ProcGen::HeightmapData> m_heightmap;

    // Terrain representation
    std::shared_ptr<SDFTerrain> m_sdfTerrain;

    // Rendering resources
    std::shared_ptr<Shader> m_terrainShader;
    std::shared_ptr<Mesh> m_terrainMesh;

    // Configuration
    float m_worldSize = 500.0f;
    int m_resolution = 1024;
    float m_heightScale = 60.0f;
    int m_seed = 42873;

    // Lighting
    glm::vec3 m_lightDirection;
    glm::vec3 m_lightColor;
    float m_ambientStrength;
    glm::vec3 m_ambientColor;

    // Atmospheric
    glm::vec3 m_fogColor;
    float m_fogDensity;
    float m_desaturationAmount;

    // Feature rendering resources
    std::shared_ptr<Shader> m_rockShader;
    std::shared_ptr<Mesh> m_rockMesh;
    std::vector<glm::mat4> m_rockTransforms;

    std::shared_ptr<Shader> m_vegetationShader;
    std::unordered_map<std::shared_ptr<Mesh>, std::vector<glm::mat4>> m_vegetationInstances;

    std::shared_ptr<Shader> m_waterShader;
    std::shared_ptr<Mesh> m_waterMesh;
    float m_waterLevel = 0.0f;
    glm::vec3 m_waterColor = glm::vec3(0.2f, 0.4f, 0.6f);
    float m_waterOpacity = 0.7f;
};

} // namespace Nova


// =============================================================================
// Example Usage in RTSApplication
// =============================================================================

/*
// In RTSApplication.hpp:
class RTSApplication : public Application {
    // ...
private:
    std::unique_ptr<Nova::MenuLandscape> m_menuLandscape;
};

// In RTSApplication::Initialize():
bool RTSApplication::Initialize() {
    // ... existing initialization ...

    // Load cinematic landscape
    m_menuLandscape = std::make_unique<Nova::MenuLandscape>();
    if (!m_menuLandscape->Initialize("game/assets/terrain/menu_landscape.json")) {
        spdlog::error("Failed to load menu landscape");
        return false;
    }

    return true;
}

// In RTSApplication::Render():
void RTSApplication::Render() {
    auto& renderer = Nova::Engine::Instance().GetRenderer();

    if (m_currentMode == GameMode::MainMenu) {
        // Render cinematic landscape
        m_menuLandscape->Render(
            m_camera->GetViewMatrix(),
            m_camera->GetProjectionMatrix(),
            m_camera->GetPosition()
        );

        // Render hero on platform
        if (m_heroMesh) {
            // Hero is placed at (-4.0, 0.0, 3.0) by the landscape system
            float heroHeight = m_menuLandscape->GetHeightAt(-4.0f, 3.0f);

            glm::mat4 heroTransform = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(-4.0f, heroHeight, 3.0f)
            );
            heroTransform = glm::rotate(
                heroTransform,
                glm::radians(155.0f),
                glm::vec3(0, 1, 0)
            );

            m_basicShader->SetMat4("u_Model", heroTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.7f, 0.6f, 0.5f));
            m_heroMesh->Draw();
        }

        // Render menu UI on top
        RenderMainMenuUI();
    }
}
*/
