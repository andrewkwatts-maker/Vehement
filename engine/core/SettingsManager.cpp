#include "SettingsManager.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <spdlog/spdlog.h>

namespace Nova {

// ============================================================================
// Enum to String Conversions
// ============================================================================

const char* QualityPresetToString(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Low: return "Low";
        case QualityPreset::Medium: return "Medium";
        case QualityPreset::High: return "High";
        case QualityPreset::Ultra: return "Ultra";
        case QualityPreset::Custom: return "Custom";
    }
    return "Unknown";
}

const char* RenderBackendToString(RenderBackend backend) {
    switch (backend) {
        case RenderBackend::SDFFirst: return "SDF-First";
        case RenderBackend::PolygonOnly: return "Polygon Only";
        case RenderBackend::GPUDriven: return "GPU-Driven";
        case RenderBackend::PathTracing: return "Path Tracing";
    }
    return "Unknown";
}

const char* GIMethodToString(GIMethod method) {
    switch (method) {
        case GIMethod::None: return "None";
        case GIMethod::ReSTIR: return "ReSTIR";
        case GIMethod::SVGF: return "SVGF";
        case GIMethod::ReSTIR_SVGF: return "ReSTIR+SVGF";
    }
    return "Unknown";
}

const char* LODQualityToString(LODQuality quality) {
    switch (quality) {
        case LODQuality::VeryLow: return "Very Low";
        case LODQuality::Low: return "Low";
        case LODQuality::Medium: return "Medium";
        case LODQuality::High: return "High";
        case LODQuality::VeryHigh: return "Very High";
    }
    return "Unknown";
}

// ============================================================================
// RenderingSettings JSON Serialization
// ============================================================================

nlohmann::json RenderingSettings::ToJson() const {
    nlohmann::json json;
    json["backend"] = static_cast<int>(backend);
    json["resolutionScale"] = resolutionScale;
    json["targetFPS"] = targetFPS;
    json["enableAdaptive"] = enableAdaptive;

    json["sdfRasterizer"] = {
        {"tileSize", {sdfTileSize.x, sdfTileSize.y}},
        {"maxSteps", maxRaymarchSteps},
        {"enableTemporal", enableTemporal},
        {"enableCheckerboard", enableCheckerboard},
        {"epsilon", raymarchEpsilon}
    };

    json["polygonRasterizer"] = {
        {"enableInstancing", enableInstancing},
        {"shadowCascades", shadowCascades},
        {"msaaSamples", msaaSamples},
        {"enableOcclusionCulling", enableOcclusionCulling}
    };

    json["gpuDriven"] = {
        {"enableGPUCulling", enableGPUCulling},
        {"jobSize", gpuCullingJobSize},
        {"persistentBuffers", enablePersistentBuffers},
        {"meshShaders", enableMeshShaders}
    };

    json["asyncCompute"] = {
        {"enable", enableAsyncCompute},
        {"overlap", asyncComputeOverlap}
    };

    return json;
}

RenderingSettings RenderingSettings::FromJson(const nlohmann::json& json) {
    RenderingSettings settings;

    if (json.contains("backend"))
        settings.backend = static_cast<RenderBackend>(json["backend"].get<int>());
    if (json.contains("resolutionScale"))
        settings.resolutionScale = json["resolutionScale"];
    if (json.contains("targetFPS"))
        settings.targetFPS = json["targetFPS"];
    if (json.contains("enableAdaptive"))
        settings.enableAdaptive = json["enableAdaptive"];

    if (json.contains("sdfRasterizer")) {
        const auto& sdf = json["sdfRasterizer"];
        if (sdf.contains("tileSize") && sdf["tileSize"].is_array())
            settings.sdfTileSize = glm::ivec2(sdf["tileSize"][0], sdf["tileSize"][1]);
        if (sdf.contains("maxSteps"))
            settings.maxRaymarchSteps = sdf["maxSteps"];
        if (sdf.contains("enableTemporal"))
            settings.enableTemporal = sdf["enableTemporal"];
        if (sdf.contains("enableCheckerboard"))
            settings.enableCheckerboard = sdf["enableCheckerboard"];
        if (sdf.contains("epsilon"))
            settings.raymarchEpsilon = sdf["epsilon"];
    }

    if (json.contains("polygonRasterizer")) {
        const auto& poly = json["polygonRasterizer"];
        if (poly.contains("enableInstancing"))
            settings.enableInstancing = poly["enableInstancing"];
        if (poly.contains("shadowCascades"))
            settings.shadowCascades = poly["shadowCascades"];
        if (poly.contains("msaaSamples"))
            settings.msaaSamples = poly["msaaSamples"];
        if (poly.contains("enableOcclusionCulling"))
            settings.enableOcclusionCulling = poly["enableOcclusionCulling"];
    }

    if (json.contains("gpuDriven")) {
        const auto& gpu = json["gpuDriven"];
        if (gpu.contains("enableGPUCulling"))
            settings.enableGPUCulling = gpu["enableGPUCulling"];
        if (gpu.contains("jobSize"))
            settings.gpuCullingJobSize = gpu["jobSize"];
        if (gpu.contains("persistentBuffers"))
            settings.enablePersistentBuffers = gpu["persistentBuffers"];
        if (gpu.contains("meshShaders"))
            settings.enableMeshShaders = gpu["meshShaders"];
    }

    if (json.contains("asyncCompute")) {
        const auto& async = json["asyncCompute"];
        if (async.contains("enable"))
            settings.enableAsyncCompute = async["enable"];
        if (async.contains("overlap"))
            settings.asyncComputeOverlap = async["overlap"];
    }

    return settings;
}

// ============================================================================
// LightingSettings JSON Serialization
// ============================================================================

nlohmann::json LightingSettings::ToJson() const {
    nlohmann::json json;

    json["clustered"] = {
        {"maxLights", maxLights},
        {"grid", {clusterGrid.x, clusterGrid.y, clusterGrid.z}},
        {"maxPerCluster", maxLightsPerCluster},
        {"enableOverflow", enableOverflowHandling}
    };

    json["shadows"] = {
        {"atlasSize", {shadowAtlasSize.x, shadowAtlasSize.y}},
        {"maxMaps", maxShadowMaps},
        {"cascadeSplits", cascadeSplits},
        {"softSamples", softShadowSamples},
        {"bias", shadowBias},
        {"contactShadows", enableContactShadows}
    };

    json["gi"] = {
        {"method", static_cast<int>(giMethod)},
        {"samplesPerPixel", giSamplesPerPixel},
        {"restirReuse", restirReusePercent},
        {"svgfIterations", svgfIterations},
        {"enableCache", enableGICache}
    };

    json["lightTypes"] = {
        {"point", enablePointLights},
        {"spot", enableSpotLights},
        {"directional", enableDirectionalLights},
        {"area", enableAreaLights},
        {"emissive", enableEmissiveGeometry}
    };

    return json;
}

LightingSettings LightingSettings::FromJson(const nlohmann::json& json) {
    LightingSettings settings;

    if (json.contains("clustered")) {
        const auto& cluster = json["clustered"];
        if (cluster.contains("maxLights"))
            settings.maxLights = cluster["maxLights"];
        if (cluster.contains("grid") && cluster["grid"].is_array())
            settings.clusterGrid = glm::ivec3(cluster["grid"][0], cluster["grid"][1], cluster["grid"][2]);
        if (cluster.contains("maxPerCluster"))
            settings.maxLightsPerCluster = cluster["maxPerCluster"];
        if (cluster.contains("enableOverflow"))
            settings.enableOverflowHandling = cluster["enableOverflow"];
    }

    if (json.contains("shadows")) {
        const auto& shadows = json["shadows"];
        if (shadows.contains("atlasSize") && shadows["atlasSize"].is_array())
            settings.shadowAtlasSize = glm::ivec2(shadows["atlasSize"][0], shadows["atlasSize"][1]);
        if (shadows.contains("maxMaps"))
            settings.maxShadowMaps = shadows["maxMaps"];
        if (shadows.contains("cascadeSplits"))
            settings.cascadeSplits = shadows["cascadeSplits"].get<std::vector<float>>();
        if (shadows.contains("softSamples"))
            settings.softShadowSamples = shadows["softSamples"];
        if (shadows.contains("bias"))
            settings.shadowBias = shadows["bias"];
        if (shadows.contains("contactShadows"))
            settings.enableContactShadows = shadows["contactShadows"];
    }

    if (json.contains("gi")) {
        const auto& gi = json["gi"];
        if (gi.contains("method"))
            settings.giMethod = static_cast<GIMethod>(gi["method"].get<int>());
        if (gi.contains("samplesPerPixel"))
            settings.giSamplesPerPixel = gi["samplesPerPixel"];
        if (gi.contains("restirReuse"))
            settings.restirReusePercent = gi["restirReuse"];
        if (gi.contains("svgfIterations"))
            settings.svgfIterations = gi["svgfIterations"];
        if (gi.contains("enableCache"))
            settings.enableGICache = gi["enableCache"];
    }

    if (json.contains("lightTypes")) {
        const auto& types = json["lightTypes"];
        if (types.contains("point"))
            settings.enablePointLights = types["point"];
        if (types.contains("spot"))
            settings.enableSpotLights = types["spot"];
        if (types.contains("directional"))
            settings.enableDirectionalLights = types["directional"];
        if (types.contains("area"))
            settings.enableAreaLights = types["area"];
        if (types.contains("emissive"))
            settings.enableEmissiveGeometry = types["emissive"];
    }

    return settings;
}

// ============================================================================
// MaterialSettings JSON Serialization
// ============================================================================

nlohmann::json MaterialSettings::ToJson() const {
    nlohmann::json json;

    json["physical"] = {
        {"ior", enableIOR},
        {"dispersion", enableDispersion},
        {"subsurface", enableSubsurfaceScattering},
        {"blackbody", enableBlackbodyEmission},
        {"clearcoat", enableClearcoat},
        {"sheen", enableSheen}
    };

    json["textures"] = {
        {"maxSize", maxTextureSize},
        {"anisotropic", anisotropicFiltering},
        {"mipmapBias", mipmapBias},
        {"compression", enableTextureCompression},
        {"virtual", enableVirtualTexturing}
    };

    json["shaders"] = {
        {"optimize", optimizeShaders},
        {"cache", cacheShaders},
        {"debugInfo", includeDebugInfo}
    };

    return json;
}

MaterialSettings MaterialSettings::FromJson(const nlohmann::json& json) {
    MaterialSettings settings;

    if (json.contains("physical")) {
        const auto& phys = json["physical"];
        if (phys.contains("ior"))
            settings.enableIOR = phys["ior"];
        if (phys.contains("dispersion"))
            settings.enableDispersion = phys["dispersion"];
        if (phys.contains("subsurface"))
            settings.enableSubsurfaceScattering = phys["subsurface"];
        if (phys.contains("blackbody"))
            settings.enableBlackbodyEmission = phys["blackbody"];
        if (phys.contains("clearcoat"))
            settings.enableClearcoat = phys["clearcoat"];
        if (phys.contains("sheen"))
            settings.enableSheen = phys["sheen"];
    }

    if (json.contains("textures")) {
        const auto& tex = json["textures"];
        if (tex.contains("maxSize"))
            settings.maxTextureSize = tex["maxSize"];
        if (tex.contains("anisotropic"))
            settings.anisotropicFiltering = tex["anisotropic"];
        if (tex.contains("mipmapBias"))
            settings.mipmapBias = tex["mipmapBias"];
        if (tex.contains("compression"))
            settings.enableTextureCompression = tex["compression"];
        if (tex.contains("virtual"))
            settings.enableVirtualTexturing = tex["virtual"];
    }

    if (json.contains("shaders")) {
        const auto& shd = json["shaders"];
        if (shd.contains("optimize"))
            settings.optimizeShaders = shd["optimize"];
        if (shd.contains("cache"))
            settings.cacheShaders = shd["cache"];
        if (shd.contains("debugInfo"))
            settings.includeDebugInfo = shd["debugInfo"];
    }

    return settings;
}

// ============================================================================
// LODSettings JSON Serialization
// ============================================================================

nlohmann::json LODSettings::ToJson() const {
    nlohmann::json json;

    json["quality"] = static_cast<int>(quality);
    json["bias"] = lodBias;
    json["distances"] = lodDistances;
    json["cullingDistance"] = cullingDistance;

    json["transition"] = {
        {"dithering", enableDithering},
        {"width", transitionWidth},
        {"hysteresis", hysteresisPercent}
    };

    auto typeToJson = [](const TypeSettings& ts) {
        nlohmann::json j;
        j["useCustom"] = ts.useCustom;
        j["customDistances"] = ts.customDistances;
        j["customCulling"] = ts.customCulling;
        return j;
    };

    json["buildings"] = typeToJson(buildings);
    json["units"] = typeToJson(units);
    json["terrain"] = typeToJson(terrain);

    return json;
}

LODSettings LODSettings::FromJson(const nlohmann::json& json) {
    LODSettings settings;

    if (json.contains("quality"))
        settings.quality = static_cast<LODQuality>(json["quality"].get<int>());
    if (json.contains("bias"))
        settings.lodBias = json["bias"];
    if (json.contains("distances"))
        settings.lodDistances = json["distances"].get<std::vector<float>>();
    if (json.contains("cullingDistance"))
        settings.cullingDistance = json["cullingDistance"];

    if (json.contains("transition")) {
        const auto& trans = json["transition"];
        if (trans.contains("dithering"))
            settings.enableDithering = trans["dithering"];
        if (trans.contains("width"))
            settings.transitionWidth = trans["width"];
        if (trans.contains("hysteresis"))
            settings.hysteresisPercent = trans["hysteresis"];
    }

    auto typeFromJson = [](const nlohmann::json& j, TypeSettings& ts) {
        if (j.contains("useCustom"))
            ts.useCustom = j["useCustom"];
        if (j.contains("customDistances"))
            ts.customDistances = j["customDistances"].get<std::vector<float>>();
        if (j.contains("customCulling"))
            ts.customCulling = j["customCulling"];
    };

    if (json.contains("buildings"))
        typeFromJson(json["buildings"], settings.buildings);
    if (json.contains("units"))
        typeFromJson(json["units"], settings.units);
    if (json.contains("terrain"))
        typeFromJson(json["terrain"], settings.terrain);

    return settings;
}

// ============================================================================
// CachingSettings JSON Serialization
// ============================================================================

nlohmann::json CachingSettings::ToJson() const {
    nlohmann::json json;

    json["brickCache"] = {
        {"enable", enableBrickCache},
        {"atlasSize", {brickAtlasSize.x, brickAtlasSize.y, brickAtlasSize.z}},
        {"maxMemoryMB", maxCacheMemoryMB},
        {"deduplication", enableDeduplication}
    };

    json["shaderCache"] = {
        {"enable", enableShaderCache},
        {"path", shaderCachePath},
        {"maxShaders", maxCachedShaders}
    };

    json["lightCache"] = {
        {"enable", enableLightCache},
        {"updateFrequency", static_cast<int>(lightCacheUpdate)},
        {"staticCache", enableStaticLightCache}
    };

    return json;
}

CachingSettings CachingSettings::FromJson(const nlohmann::json& json) {
    CachingSettings settings;

    if (json.contains("brickCache")) {
        const auto& brick = json["brickCache"];
        if (brick.contains("enable"))
            settings.enableBrickCache = brick["enable"];
        if (brick.contains("atlasSize") && brick["atlasSize"].is_array())
            settings.brickAtlasSize = glm::ivec3(brick["atlasSize"][0], brick["atlasSize"][1], brick["atlasSize"][2]);
        if (brick.contains("maxMemoryMB"))
            settings.maxCacheMemoryMB = brick["maxMemoryMB"];
        if (brick.contains("deduplication"))
            settings.enableDeduplication = brick["deduplication"];
    }

    if (json.contains("shaderCache")) {
        const auto& shader = json["shaderCache"];
        if (shader.contains("enable"))
            settings.enableShaderCache = shader["enable"];
        if (shader.contains("path"))
            settings.shaderCachePath = shader["path"];
        if (shader.contains("maxShaders"))
            settings.maxCachedShaders = shader["maxShaders"];
    }

    if (json.contains("lightCache")) {
        const auto& light = json["lightCache"];
        if (light.contains("enable"))
            settings.enableLightCache = light["enable"];
        if (light.contains("updateFrequency"))
            settings.lightCacheUpdate = static_cast<UpdateFrequency>(light["updateFrequency"].get<int>());
        if (light.contains("staticCache"))
            settings.enableStaticLightCache = light["staticCache"];
    }

    return settings;
}

// ============================================================================
// PerformanceSettings JSON Serialization
// ============================================================================

nlohmann::json PerformanceSettings::ToJson() const {
    nlohmann::json json;

    json["threadPool"] = {
        {"workerThreads", workerThreads},
        {"jobQueueSize", jobQueueSize}
    };

    json["memory"] = {
        {"gpuMemoryLimitMB", gpuMemoryLimitMB},
        {"streamingBudgetMB", streamingBudgetMB}
    };

    json["profiling"] = {
        {"enable", enableProfiler},
        {"overlay", showProfilerOverlay},
        {"exportCSV", exportCSV},
        {"outputPath", profileOutputPath}
    };

    return json;
}

PerformanceSettings PerformanceSettings::FromJson(const nlohmann::json& json) {
    PerformanceSettings settings;

    if (json.contains("threadPool")) {
        const auto& pool = json["threadPool"];
        if (pool.contains("workerThreads"))
            settings.workerThreads = pool["workerThreads"];
        if (pool.contains("jobQueueSize"))
            settings.jobQueueSize = pool["jobQueueSize"];
    }

    if (json.contains("memory")) {
        const auto& mem = json["memory"];
        if (mem.contains("gpuMemoryLimitMB"))
            settings.gpuMemoryLimitMB = mem["gpuMemoryLimitMB"];
        if (mem.contains("streamingBudgetMB"))
            settings.streamingBudgetMB = mem["streamingBudgetMB"];
    }

    if (json.contains("profiling")) {
        const auto& prof = json["profiling"];
        if (prof.contains("enable"))
            settings.enableProfiler = prof["enable"];
        if (prof.contains("overlay"))
            settings.showProfilerOverlay = prof["overlay"];
        if (prof.contains("exportCSV"))
            settings.exportCSV = prof["exportCSV"];
        if (prof.contains("outputPath"))
            settings.profileOutputPath = prof["outputPath"];
    }

    return settings;
}

// ============================================================================
// CompleteSettings JSON Serialization
// ============================================================================

nlohmann::json CompleteSettings::ToJson() const {
    nlohmann::json json;
    json["preset"] = static_cast<int>(preset);
    json["rendering"] = rendering.ToJson();
    json["lighting"] = lighting.ToJson();
    json["materials"] = materials.ToJson();
    json["lod"] = lod.ToJson();
    json["caching"] = caching.ToJson();
    json["performance"] = performance.ToJson();
    return json;
}

CompleteSettings CompleteSettings::FromJson(const nlohmann::json& json) {
    CompleteSettings settings;

    if (json.contains("preset"))
        settings.preset = static_cast<QualityPreset>(json["preset"].get<int>());
    if (json.contains("rendering"))
        settings.rendering = RenderingSettings::FromJson(json["rendering"]);
    if (json.contains("lighting"))
        settings.lighting = LightingSettings::FromJson(json["lighting"]);
    if (json.contains("materials"))
        settings.materials = MaterialSettings::FromJson(json["materials"]);
    if (json.contains("lod"))
        settings.lod = LODSettings::FromJson(json["lod"]);
    if (json.contains("caching"))
        settings.caching = CachingSettings::FromJson(json["caching"]);
    if (json.contains("performance"))
        settings.performance = PerformanceSettings::FromJson(json["performance"]);

    return settings;
}

// ============================================================================
// SettingsManager Implementation
// ============================================================================

SettingsManager& SettingsManager::Instance() {
    static SettingsManager instance;
    return instance;
}

void SettingsManager::Initialize() {
    m_settings = GetPresetSettings(QualityPreset::High);
    spdlog::info("SettingsManager initialized with High preset");
}

std::expected<void, std::string> SettingsManager::Load(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected("Failed to open file: " + filepath);
        }

        nlohmann::json json;
        file >> json;
        file.close();

        m_settings = CompleteSettings::FromJson(json);
        spdlog::info("Settings loaded from: {}", filepath);

        NotifyChanges();
        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to load settings: " + std::string(e.what()));
    }
}

std::expected<void, std::string> SettingsManager::Save(const std::string& filepath) {
    try {
        nlohmann::json json = m_settings.ToJson();

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected("Failed to create file: " + filepath);
        }

        file << json.dump(2);
        file.close();

        spdlog::info("Settings saved to: {}", filepath);
        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to save settings: " + std::string(e.what()));
    }
}

void SettingsManager::ApplyPreset(QualityPreset preset) {
    m_settings = GetPresetSettings(preset);
    m_settings.preset = preset;
    spdlog::info("Applied preset: {}", QualityPresetToString(preset));
    NotifyChanges();
}

CompleteSettings SettingsManager::GetPresetSettings(QualityPreset preset) {
    CompleteSettings settings;
    settings.preset = preset;

    switch (preset) {
        case QualityPreset::Low:
            // Rendering
            settings.rendering.resolutionScale = 50;
            settings.rendering.targetFPS = 30;
            settings.rendering.maxRaymarchSteps = 64;
            settings.rendering.enableTemporal = true;
            settings.rendering.enableCheckerboard = true;
            settings.rendering.shadowCascades = 2;
            settings.rendering.msaaSamples = 0;

            // Lighting
            settings.lighting.maxLights = 10000;
            settings.lighting.shadowAtlasSize = glm::ivec2(4096, 4096);
            settings.lighting.maxShadowMaps = 64;
            settings.lighting.giMethod = GIMethod::None;
            settings.lighting.softShadowSamples = 4;

            // Materials
            settings.materials.maxTextureSize = 1024;
            settings.materials.anisotropicFiltering = 4;
            settings.materials.enableSubsurfaceScattering = false;
            settings.materials.enableDispersion = false;

            // LOD
            settings.lod.quality = LODQuality::Low;
            settings.lod.lodDistances = {5.0f, 15.0f, 30.0f, 60.0f};
            settings.lod.cullingDistance = 100.0f;

            // Caching
            settings.caching.brickAtlasSize = glm::ivec3(16, 16, 16);
            settings.caching.maxCacheMemoryMB = 256;
            settings.caching.maxCachedShaders = 256;

            // Performance
            settings.performance.workerThreads = 4;
            settings.performance.streamingBudgetMB = 512;
            break;

        case QualityPreset::Medium:
            // Rendering
            settings.rendering.resolutionScale = 75;
            settings.rendering.targetFPS = 60;
            settings.rendering.maxRaymarchSteps = 96;
            settings.rendering.enableTemporal = true;
            settings.rendering.enableCheckerboard = true;
            settings.rendering.shadowCascades = 3;
            settings.rendering.msaaSamples = 2;

            // Lighting
            settings.lighting.maxLights = 50000;
            settings.lighting.shadowAtlasSize = glm::ivec2(8192, 8192);
            settings.lighting.maxShadowMaps = 128;
            settings.lighting.giMethod = GIMethod::SVGF;
            settings.lighting.softShadowSamples = 8;

            // Materials
            settings.materials.maxTextureSize = 2048;
            settings.materials.anisotropicFiltering = 8;
            settings.materials.enableSubsurfaceScattering = true;
            settings.materials.enableDispersion = false;

            // LOD
            settings.lod.quality = LODQuality::Medium;
            settings.lod.lodDistances = {8.0f, 20.0f, 40.0f, 80.0f};
            settings.lod.cullingDistance = 150.0f;

            // Caching
            settings.caching.brickAtlasSize = glm::ivec3(24, 24, 24);
            settings.caching.maxCacheMemoryMB = 384;
            settings.caching.maxCachedShaders = 512;

            // Performance
            settings.performance.workerThreads = 6;
            settings.performance.streamingBudgetMB = 1024;
            break;

        case QualityPreset::High:
            // Rendering
            settings.rendering.resolutionScale = 100;
            settings.rendering.targetFPS = 60;
            settings.rendering.maxRaymarchSteps = 128;
            settings.rendering.enableTemporal = true;
            settings.rendering.enableCheckerboard = false;
            settings.rendering.shadowCascades = 4;
            settings.rendering.msaaSamples = 4;

            // Lighting
            settings.lighting.maxLights = 100000;
            settings.lighting.shadowAtlasSize = glm::ivec2(16384, 16384);
            settings.lighting.maxShadowMaps = 256;
            settings.lighting.giMethod = GIMethod::ReSTIR_SVGF;
            settings.lighting.softShadowSamples = 16;

            // Materials
            settings.materials.maxTextureSize = 4096;
            settings.materials.anisotropicFiltering = 16;
            settings.materials.enableSubsurfaceScattering = true;
            settings.materials.enableDispersion = true;

            // LOD
            settings.lod.quality = LODQuality::High;
            settings.lod.lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
            settings.lod.cullingDistance = 200.0f;

            // Caching
            settings.caching.brickAtlasSize = glm::ivec3(32, 32, 32);
            settings.caching.maxCacheMemoryMB = 512;
            settings.caching.maxCachedShaders = 1000;

            // Performance
            settings.performance.workerThreads = 8;
            settings.performance.streamingBudgetMB = 2048;
            break;

        case QualityPreset::Ultra:
            // Rendering
            settings.rendering.resolutionScale = 100;
            settings.rendering.targetFPS = 120;
            settings.rendering.maxRaymarchSteps = 256;
            settings.rendering.enableTemporal = true;
            settings.rendering.enableCheckerboard = false;
            settings.rendering.shadowCascades = 6;
            settings.rendering.msaaSamples = 8;

            // Lighting
            settings.lighting.maxLights = 250000;
            settings.lighting.shadowAtlasSize = glm::ivec2(32768, 32768);
            settings.lighting.maxShadowMaps = 512;
            settings.lighting.giMethod = GIMethod::ReSTIR_SVGF;
            settings.lighting.softShadowSamples = 32;

            // Materials
            settings.materials.maxTextureSize = 8192;
            settings.materials.anisotropicFiltering = 16;
            settings.materials.enableSubsurfaceScattering = true;
            settings.materials.enableDispersion = true;
            settings.materials.enableVirtualTexturing = true;

            // LOD
            settings.lod.quality = LODQuality::VeryHigh;
            settings.lod.lodDistances = {15.0f, 40.0f, 80.0f, 150.0f};
            settings.lod.cullingDistance = 300.0f;

            // Caching
            settings.caching.brickAtlasSize = glm::ivec3(48, 48, 48);
            settings.caching.maxCacheMemoryMB = 1024;
            settings.caching.maxCachedShaders = 2000;

            // Performance
            settings.performance.workerThreads = 0;  // Auto-detect
            settings.performance.streamingBudgetMB = 4096;
            break;

        case QualityPreset::Custom:
            // Keep current settings
            break;
    }

    // Auto-detect worker threads if set to 0
    if (settings.performance.workerThreads == 0) {
        settings.performance.workerThreads = std::max(1u, std::thread::hardware_concurrency());
    }

    return settings;
}

ValidationResult SettingsManager::Validate() const {
    ValidationResult result;

    // Validate rendering settings
    if (m_settings.rendering.resolutionScale < 10 || m_settings.rendering.resolutionScale > 200) {
        result.AddWarning("Resolution scale should be between 10% and 200%");
    }
    if (m_settings.rendering.maxRaymarchSteps < 16 || m_settings.rendering.maxRaymarchSteps > 512) {
        result.AddWarning("Raymarch steps should be between 16 and 512");
    }

    // Validate lighting settings
    if (m_settings.lighting.maxLights < 100 || m_settings.lighting.maxLights > 1000000) {
        result.AddError("Max lights must be between 100 and 1,000,000");
    }
    if (m_settings.lighting.shadowAtlasSize.x != m_settings.lighting.shadowAtlasSize.y) {
        result.AddWarning("Shadow atlas should be square for optimal performance");
    }

    // Validate material settings
    if (m_settings.materials.maxTextureSize < 256 || m_settings.materials.maxTextureSize > 16384) {
        result.AddError("Max texture size must be between 256 and 16384");
    }

    // Validate LOD settings
    if (m_settings.lod.lodDistances.empty()) {
        result.AddError("LOD distances array cannot be empty");
    }
    for (size_t i = 1; i < m_settings.lod.lodDistances.size(); ++i) {
        if (m_settings.lod.lodDistances[i] <= m_settings.lod.lodDistances[i - 1]) {
            result.AddError("LOD distances must be in ascending order");
        }
    }

    // Validate caching settings
    if (m_settings.caching.maxCacheMemoryMB < 64 || m_settings.caching.maxCacheMemoryMB > 8192) {
        result.AddWarning("Cache memory should be between 64 MB and 8 GB");
    }

    // Validate performance settings
    if (m_settings.performance.workerThreads < 1 || m_settings.performance.workerThreads > 64) {
        result.AddWarning("Worker threads should be between 1 and 64");
    }

    return result;
}

void SettingsManager::NotifyChanges() {
    for (auto& callback : m_changeCallbacks) {
        callback(m_settings);
    }
}

} // namespace Nova
