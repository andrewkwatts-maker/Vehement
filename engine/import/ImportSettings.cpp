#include "ImportSettings.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Import Settings Base Implementation
// ============================================================================

std::string ImportSettingsBase::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"assetPath\": \"" << assetPath << "\",\n";
    ss << "  \"outputPath\": \"" << outputPath << "\",\n";
    ss << "  \"assetId\": \"" << assetId << "\",\n";
    ss << "  \"settingsVersion\": " << settingsVersion << ",\n";
    ss << "  \"sourceFileHash\": " << sourceFileHash << ",\n";
    ss << "  \"lastImportTime\": " << lastImportTime << ",\n";
    ss << "  \"preset\": " << static_cast<int>(preset) << ",\n";
    ss << "  \"targetPlatform\": " << static_cast<int>(targetPlatform) << ",\n";
    ss << "  \"enabled\": " << (enabled ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

bool ImportSettingsBase::FromJson(const std::string& json) {
    // Simple JSON parsing for base fields
    auto getValue = [&json](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";

        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

        if (json[pos] == '"') {
            size_t start = pos + 1;
            size_t end = json.find('"', start);
            return json.substr(start, end - start);
        } else {
            size_t start = pos;
            size_t end = json.find_first_of(",}\n", start);
            return json.substr(start, end - start);
        }
    };

    assetPath = getValue("assetPath");
    outputPath = getValue("outputPath");
    assetId = getValue("assetId");

    std::string val = getValue("settingsVersion");
    if (!val.empty()) settingsVersion = std::stoul(val);

    val = getValue("sourceFileHash");
    if (!val.empty()) sourceFileHash = std::stoull(val);

    val = getValue("lastImportTime");
    if (!val.empty()) lastImportTime = std::stoull(val);

    val = getValue("preset");
    if (!val.empty()) preset = static_cast<ImportPreset>(std::stoi(val));

    val = getValue("targetPlatform");
    if (!val.empty()) targetPlatform = static_cast<TargetPlatform>(std::stoi(val));

    val = getValue("enabled");
    enabled = (val == "true" || val == "1");

    return true;
}

void ImportSettingsBase::ApplyPreset(ImportPreset newPreset) {
    preset = newPreset;
}

// ============================================================================
// Texture Import Settings Implementation
// ============================================================================

std::unique_ptr<ImportSettingsBase> TextureImportSettings::Clone() const {
    return std::make_unique<TextureImportSettings>(*this);
}

std::string TextureImportSettings::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"type\": \"Texture\",\n";

    // Base fields
    ss << "  \"assetPath\": \"" << assetPath << "\",\n";
    ss << "  \"outputPath\": \"" << outputPath << "\",\n";
    ss << "  \"assetId\": \"" << assetId << "\",\n";
    ss << "  \"settingsVersion\": " << settingsVersion << ",\n";
    ss << "  \"sourceFileHash\": " << sourceFileHash << ",\n";
    ss << "  \"lastImportTime\": " << lastImportTime << ",\n";
    ss << "  \"preset\": " << static_cast<int>(preset) << ",\n";
    ss << "  \"targetPlatform\": " << static_cast<int>(targetPlatform) << ",\n";
    ss << "  \"enabled\": " << (enabled ? "true" : "false") << ",\n";

    // Texture-specific fields
    ss << "  \"textureType\": " << static_cast<int>(textureType) << ",\n";
    ss << "  \"compression\": " << static_cast<int>(compression) << ",\n";
    ss << "  \"sRGB\": " << (sRGB ? "true" : "false") << ",\n";
    ss << "  \"generateMipmaps\": " << (generateMipmaps ? "true" : "false") << ",\n";
    ss << "  \"mipmapQuality\": " << static_cast<int>(mipmapQuality) << ",\n";
    ss << "  \"maxMipLevel\": " << maxMipLevel << ",\n";
    ss << "  \"maxWidth\": " << maxWidth << ",\n";
    ss << "  \"maxHeight\": " << maxHeight << ",\n";
    ss << "  \"powerOfTwo\": " << (powerOfTwo ? "true" : "false") << ",\n";
    ss << "  \"enableAnisotropic\": " << (enableAnisotropic ? "true" : "false") << ",\n";
    ss << "  \"anisotropicLevel\": " << anisotropicLevel << ",\n";
    ss << "  \"premultiplyAlpha\": " << (premultiplyAlpha ? "true" : "false") << ",\n";
    ss << "  \"flipVertically\": " << (flipVertically ? "true" : "false") << ",\n";
    ss << "  \"isNormalMap\": " << (isNormalMap ? "true" : "false") << ",\n";
    ss << "  \"normalMapStrength\": " << normalMapStrength << ",\n";
    ss << "  \"createAtlas\": " << (createAtlas ? "true" : "false") << ",\n";
    ss << "  \"atlasMaxSize\": " << atlasMaxSize << ",\n";
    ss << "  \"atlasPadding\": " << atlasPadding << ",\n";
    ss << "  \"sliceSprites\": " << (sliceSprites ? "true" : "false") << ",\n";
    ss << "  \"sliceWidth\": " << sliceWidth << ",\n";
    ss << "  \"sliceHeight\": " << sliceHeight << ",\n";
    ss << "  \"generateThumbnail\": " << (generateThumbnail ? "true" : "false") << ",\n";
    ss << "  \"thumbnailSize\": " << thumbnailSize << ",\n";
    ss << "  \"compressionQuality\": " << compressionQuality << ",\n";
    ss << "  \"enableStreaming\": " << (enableStreaming ? "true" : "false") << "\n";
    ss << "}";

    return ss.str();
}

bool TextureImportSettings::FromJson(const std::string& json) {
    ImportSettingsBase::FromJson(json);

    auto getInt = [&json](const std::string& key, int defaultVal) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        try {
            return std::stoi(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    auto getBool = [&json](const std::string& key, bool defaultVal) -> bool {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        return json.substr(pos, 4) == "true";
    };

    auto getFloat = [&json](const std::string& key, float defaultVal) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        try {
            return std::stof(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    textureType = static_cast<TextureType>(getInt("textureType", 0));
    compression = static_cast<TextureCompression>(getInt("compression", 7));
    sRGB = getBool("sRGB", true);
    generateMipmaps = getBool("generateMipmaps", true);
    mipmapQuality = static_cast<MipmapQuality>(getInt("mipmapQuality", 1));
    maxMipLevel = getInt("maxMipLevel", 0);
    maxWidth = getInt("maxWidth", 4096);
    maxHeight = getInt("maxHeight", 4096);
    powerOfTwo = getBool("powerOfTwo", false);
    enableAnisotropic = getBool("enableAnisotropic", true);
    anisotropicLevel = getInt("anisotropicLevel", 8);
    premultiplyAlpha = getBool("premultiplyAlpha", false);
    flipVertically = getBool("flipVertically", false);
    isNormalMap = getBool("isNormalMap", false);
    normalMapStrength = getFloat("normalMapStrength", 1.0f);
    createAtlas = getBool("createAtlas", false);
    atlasMaxSize = getInt("atlasMaxSize", 4096);
    atlasPadding = getInt("atlasPadding", 2);
    sliceSprites = getBool("sliceSprites", false);
    sliceWidth = getInt("sliceWidth", 32);
    sliceHeight = getInt("sliceHeight", 32);
    generateThumbnail = getBool("generateThumbnail", true);
    thumbnailSize = getInt("thumbnailSize", 128);
    compressionQuality = getInt("compressionQuality", 75);
    enableStreaming = getBool("enableStreaming", false);

    return true;
}

void TextureImportSettings::ApplyPreset(ImportPreset newPreset) {
    ImportSettingsBase::ApplyPreset(newPreset);

    switch (newPreset) {
        case ImportPreset::Mobile:
            maxWidth = 1024;
            maxHeight = 1024;
            compression = TextureCompression::ETC2_RGBA;
            mipmapQuality = MipmapQuality::Fast;
            compressionQuality = 60;
            enableAnisotropic = false;
            enableStreaming = true;
            break;

        case ImportPreset::Desktop:
            maxWidth = 2048;
            maxHeight = 2048;
            compression = TextureCompression::BC7;
            mipmapQuality = MipmapQuality::Standard;
            compressionQuality = 75;
            enableAnisotropic = true;
            anisotropicLevel = 8;
            break;

        case ImportPreset::HighQuality:
            maxWidth = 4096;
            maxHeight = 4096;
            compression = TextureCompression::BC7;
            mipmapQuality = MipmapQuality::HighQuality;
            compressionQuality = 100;
            enableAnisotropic = true;
            anisotropicLevel = 16;
            break;

        case ImportPreset::WebGL:
            maxWidth = 1024;
            maxHeight = 1024;
            compression = TextureCompression::None;  // WebGL has limited compression
            mipmapQuality = MipmapQuality::Fast;
            compressionQuality = 70;
            enableStreaming = true;
            break;

        default:
            break;
    }
}

void TextureImportSettings::AutoDetectType(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("_n.") != std::string::npos ||
        lower.find("_normal") != std::string::npos ||
        lower.find("_nrm") != std::string::npos ||
        lower.find("_norm") != std::string::npos) {
        textureType = TextureType::Normal;
        isNormalMap = true;
        sRGB = false;
        compression = TextureCompression::BC5;
    }
    else if (lower.find("_d.") != std::string::npos ||
             lower.find("_diff") != std::string::npos ||
             lower.find("_albedo") != std::string::npos ||
             lower.find("_color") != std::string::npos) {
        textureType = TextureType::Diffuse;
        sRGB = true;
    }
    else if (lower.find("_s.") != std::string::npos ||
             lower.find("_spec") != std::string::npos ||
             lower.find("_gloss") != std::string::npos) {
        textureType = TextureType::Specular;
        sRGB = false;
    }
    else if (lower.find("_m.") != std::string::npos ||
             lower.find("_metal") != std::string::npos) {
        textureType = TextureType::Metallic;
        sRGB = false;
    }
    else if (lower.find("_r.") != std::string::npos ||
             lower.find("_rough") != std::string::npos) {
        textureType = TextureType::Roughness;
        sRGB = false;
    }
    else if (lower.find("_ao") != std::string::npos ||
             lower.find("_ambient") != std::string::npos ||
             lower.find("_occlusion") != std::string::npos) {
        textureType = TextureType::AO;
        sRGB = false;
    }
    else if (lower.find("_e.") != std::string::npos ||
             lower.find("_emit") != std::string::npos ||
             lower.find("_emissive") != std::string::npos) {
        textureType = TextureType::Emissive;
        sRGB = true;
    }
    else if (lower.find("_h.") != std::string::npos ||
             lower.find("_height") != std::string::npos ||
             lower.find("_disp") != std::string::npos ||
             lower.find("_bump") != std::string::npos) {
        textureType = TextureType::Height;
        sRGB = false;
    }
    else if (lower.find(".hdr") != std::string::npos ||
             lower.find(".exr") != std::string::npos) {
        textureType = TextureType::HDR;
        sRGB = false;
        compression = TextureCompression::BC6H;
    }
}

TextureCompression TextureImportSettings::GetRecommendedCompression(TargetPlatform platform) const {
    bool hasAlpha = true;  // Assume alpha for safety

    switch (platform) {
        case TargetPlatform::Desktop:
            if (isNormalMap) return TextureCompression::BC5;
            if (textureType == TextureType::HDR) return TextureCompression::BC6H;
            return hasAlpha ? TextureCompression::BC7 : TextureCompression::BC1;

        case TargetPlatform::Mobile:
            if (isNormalMap) return TextureCompression::ETC2_RGB;
            return hasAlpha ? TextureCompression::ASTC_4x4 : TextureCompression::ETC2_RGB;

        case TargetPlatform::WebGL:
            return TextureCompression::None;  // Limited support

        case TargetPlatform::Console:
            if (isNormalMap) return TextureCompression::BC5;
            return TextureCompression::BC7;

        default:
            return TextureCompression::BC7;
    }
}

// ============================================================================
// Model Import Settings Implementation
// ============================================================================

std::unique_ptr<ImportSettingsBase> ModelImportSettings::Clone() const {
    return std::make_unique<ModelImportSettings>(*this);
}

std::string ModelImportSettings::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"type\": \"Model\",\n";

    // Base fields
    ss << "  \"assetPath\": \"" << assetPath << "\",\n";
    ss << "  \"settingsVersion\": " << settingsVersion << ",\n";
    ss << "  \"preset\": " << static_cast<int>(preset) << ",\n";

    // Model-specific fields
    ss << "  \"scaleFactor\": " << scaleFactor << ",\n";
    ss << "  \"sourceUnits\": " << static_cast<int>(sourceUnits) << ",\n";
    ss << "  \"targetUnits\": " << static_cast<int>(targetUnits) << ",\n";
    ss << "  \"swapYZ\": " << (swapYZ ? "true" : "false") << ",\n";
    ss << "  \"optimizeMesh\": " << (optimizeMesh ? "true" : "false") << ",\n";
    ss << "  \"generateNormals\": " << (generateNormals ? "true" : "false") << ",\n";
    ss << "  \"generateTangents\": " << (generateTangents ? "true" : "false") << ",\n";
    ss << "  \"generateLODs\": " << (generateLODs ? "true" : "false") << ",\n";
    ss << "  \"importMaterials\": " << (importMaterials ? "true" : "false") << ",\n";
    ss << "  \"importTextures\": " << (importTextures ? "true" : "false") << ",\n";
    ss << "  \"importSkeleton\": " << (importSkeleton ? "true" : "false") << ",\n";
    ss << "  \"importAnimations\": " << (importAnimations ? "true" : "false") << ",\n";
    ss << "  \"generateCollision\": " << (generateCollision ? "true" : "false") << ",\n";
    ss << "  \"convexDecomposition\": " << (convexDecomposition ? "true" : "false") << ",\n";
    ss << "  \"maxBonesPerVertex\": " << maxBonesPerVertex << "\n";
    ss << "}";

    return ss.str();
}

bool ModelImportSettings::FromJson(const std::string& json) {
    ImportSettingsBase::FromJson(json);

    auto getInt = [&json](const std::string& key, int defaultVal) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        try {
            return std::stoi(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    auto getBool = [&json](const std::string& key, bool defaultVal) -> bool {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        return json.substr(pos, 4) == "true";
    };

    auto getFloat = [&json](const std::string& key, float defaultVal) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        try {
            return std::stof(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    scaleFactor = getFloat("scaleFactor", 1.0f);
    sourceUnits = static_cast<ModelUnits>(getInt("sourceUnits", 0));
    targetUnits = static_cast<ModelUnits>(getInt("targetUnits", 0));
    swapYZ = getBool("swapYZ", false);
    optimizeMesh = getBool("optimizeMesh", true);
    generateNormals = getBool("generateNormals", false);
    generateTangents = getBool("generateTangents", true);
    generateLODs = getBool("generateLODs", false);
    importMaterials = getBool("importMaterials", true);
    importTextures = getBool("importTextures", true);
    importSkeleton = getBool("importSkeleton", true);
    importAnimations = getBool("importAnimations", true);
    generateCollision = getBool("generateCollision", false);
    convexDecomposition = getBool("convexDecomposition", false);
    maxBonesPerVertex = getInt("maxBonesPerVertex", 4);

    return true;
}

void ModelImportSettings::ApplyPreset(ImportPreset newPreset) {
    ImportSettingsBase::ApplyPreset(newPreset);

    switch (newPreset) {
        case ImportPreset::Mobile:
            optimizeMesh = true;
            generateLODs = true;
            lodDistances = {5.0f, 15.0f, 30.0f};
            lodReductions = {0.5f, 0.25f, 0.1f};
            maxBonesPerVertex = 4;
            compressVertices = true;
            use16BitIndices = true;
            break;

        case ImportPreset::Desktop:
            optimizeMesh = true;
            generateLODs = true;
            lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
            lodReductions = {0.5f, 0.25f, 0.125f, 0.0625f};
            maxBonesPerVertex = 4;
            break;

        case ImportPreset::HighQuality:
            optimizeMesh = false;  // Preserve original
            generateLODs = false;
            maxBonesPerVertex = 8;
            compressVertices = false;
            break;

        default:
            break;
    }
}

float ModelImportSettings::CalculateUnitScale() const {
    static const float unitToMeters[] = {
        1.0f,       // Meters
        0.01f,      // Centimeters
        0.001f,     // Millimeters
        0.0254f,    // Inches
        0.3048f     // Feet
    };

    float sourceScale = unitToMeters[static_cast<int>(sourceUnits)];
    float targetScale = unitToMeters[static_cast<int>(targetUnits)];

    return (sourceScale / targetScale) * scaleFactor;
}

// ============================================================================
// Animation Import Settings Implementation
// ============================================================================

std::unique_ptr<ImportSettingsBase> AnimationImportSettings::Clone() const {
    return std::make_unique<AnimationImportSettings>(*this);
}

std::string AnimationImportSettings::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"type\": \"Animation\",\n";

    // Base fields
    ss << "  \"assetPath\": \"" << assetPath << "\",\n";
    ss << "  \"settingsVersion\": " << settingsVersion << ",\n";
    ss << "  \"preset\": " << static_cast<int>(preset) << ",\n";

    // Animation-specific fields
    ss << "  \"sampleRate\": " << sampleRate << ",\n";
    ss << "  \"resample\": " << (resample ? "true" : "false") << ",\n";
    ss << "  \"targetSampleRate\": " << targetSampleRate << ",\n";
    ss << "  \"compression\": " << static_cast<int>(compression) << ",\n";
    ss << "  \"positionTolerance\": " << positionTolerance << ",\n";
    ss << "  \"rotationTolerance\": " << rotationTolerance << ",\n";
    ss << "  \"extractRootMotion\": " << (extractRootMotion ? "true" : "false") << ",\n";
    ss << "  \"rootBoneName\": \"" << rootBoneName << "\",\n";
    ss << "  \"splitByMarkers\": " << (splitByMarkers ? "true" : "false") << ",\n";
    ss << "  \"detectLoops\": " << (detectLoops ? "true" : "false") << ",\n";
    ss << "  \"makeAdditive\": " << (makeAdditive ? "true" : "false") << ",\n";
    ss << "  \"enableRetargeting\": " << (enableRetargeting ? "true" : "false") << "\n";
    ss << "}";

    return ss.str();
}

bool AnimationImportSettings::FromJson(const std::string& json) {
    ImportSettingsBase::FromJson(json);

    auto getInt = [&json](const std::string& key, int defaultVal) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        try {
            return std::stoi(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    auto getBool = [&json](const std::string& key, bool defaultVal) -> bool {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        return json.substr(pos, 4) == "true";
    };

    auto getFloat = [&json](const std::string& key, float defaultVal) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        try {
            return std::stof(json.substr(pos));
        } catch (...) {
            return defaultVal;
        }
    };

    auto getString = [&json](const std::string& key, const std::string& defaultVal) -> std::string {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (json[pos] == '"') {
            size_t start = pos + 1;
            size_t end = json.find('"', start);
            return json.substr(start, end - start);
        }
        return defaultVal;
    };

    sampleRate = getFloat("sampleRate", 30.0f);
    resample = getBool("resample", false);
    targetSampleRate = getFloat("targetSampleRate", 30.0f);
    compression = static_cast<AnimationCompression>(getInt("compression", 1));
    positionTolerance = getFloat("positionTolerance", 0.001f);
    rotationTolerance = getFloat("rotationTolerance", 0.0001f);
    extractRootMotion = getBool("extractRootMotion", true);
    rootBoneName = getString("rootBoneName", "root");
    splitByMarkers = getBool("splitByMarkers", true);
    detectLoops = getBool("detectLoops", true);
    makeAdditive = getBool("makeAdditive", false);
    enableRetargeting = getBool("enableRetargeting", false);

    return true;
}

void AnimationImportSettings::ApplyPreset(ImportPreset newPreset) {
    ImportSettingsBase::ApplyPreset(newPreset);

    switch (newPreset) {
        case ImportPreset::Mobile:
            compression = AnimationCompression::Aggressive;
            positionTolerance = 0.005f;
            rotationTolerance = 0.001f;
            resample = true;
            targetSampleRate = 24.0f;
            break;

        case ImportPreset::Desktop:
            compression = AnimationCompression::Lossy;
            positionTolerance = 0.001f;
            rotationTolerance = 0.0001f;
            resample = false;
            break;

        case ImportPreset::HighQuality:
            compression = AnimationCompression::None;
            positionTolerance = 0.0f;
            rotationTolerance = 0.0f;
            resample = false;
            break;

        default:
            break;
    }
}

void AnimationImportSettings::AddClipRange(const std::string& name, float startTime, float endTime) {
    clipRanges.emplace_back(name, std::make_pair(startTime, endTime));
}

void AnimationImportSettings::AddBoneMapping(const std::string& sourceBone, const std::string& targetBone) {
    boneMapping[sourceBone] = targetBone;
}

// ============================================================================
// Import Settings Manager Implementation
// ============================================================================

ImportSettingsManager& ImportSettingsManager::Instance() {
    static ImportSettingsManager instance;
    return instance;
}

ImportSettingsBase* ImportSettingsManager::GetOrCreateSettings(const std::string& assetPath, const std::string& type) {
    auto it = m_settings.find(assetPath);
    if (it != m_settings.end()) {
        return it->second.get();
    }

    std::unique_ptr<ImportSettingsBase> settings;
    if (type == "Texture") {
        settings = std::make_unique<TextureImportSettings>();
    } else if (type == "Model") {
        settings = std::make_unique<ModelImportSettings>();
    } else if (type == "Animation") {
        settings = std::make_unique<AnimationImportSettings>();
    } else {
        return nullptr;
    }

    settings->assetPath = assetPath;
    ImportSettingsBase* ptr = settings.get();
    m_settings[assetPath] = std::move(settings);
    return ptr;
}

bool ImportSettingsManager::SaveSettings(const std::string& assetPath, const ImportSettingsBase* settings) {
    if (!settings) return false;

    std::string settingsPath = GetSettingsPath(assetPath);

    // Create directory if needed
    fs::path dir = fs::path(settingsPath).parent_path();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::ofstream file(settingsPath);
    if (!file.is_open()) return false;

    file << settings->ToJson();
    return true;
}

std::unique_ptr<ImportSettingsBase> ImportSettingsManager::LoadSettings(const std::string& assetPath) {
    std::string settingsPath = GetSettingsPath(assetPath);

    std::ifstream file(settingsPath);
    if (!file.is_open()) return nullptr;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Detect type from JSON
    std::string type = DetectAssetType(assetPath);
    if (json.find("\"type\": \"Texture\"") != std::string::npos) {
        type = "Texture";
    } else if (json.find("\"type\": \"Model\"") != std::string::npos) {
        type = "Model";
    } else if (json.find("\"type\": \"Animation\"") != std::string::npos) {
        type = "Animation";
    }

    std::unique_ptr<ImportSettingsBase> settings;
    if (type == "Texture") {
        settings = std::make_unique<TextureImportSettings>();
    } else if (type == "Model") {
        settings = std::make_unique<ModelImportSettings>();
    } else if (type == "Animation") {
        settings = std::make_unique<AnimationImportSettings>();
    } else {
        return nullptr;
    }

    settings->FromJson(json);
    return settings;
}

bool ImportSettingsManager::HasSettings(const std::string& assetPath) const {
    return m_settings.find(assetPath) != m_settings.end() ||
           fs::exists(GetSettingsPath(assetPath));
}

void ImportSettingsManager::RemoveSettings(const std::string& assetPath) {
    m_settings.erase(assetPath);
    std::string settingsPath = GetSettingsPath(assetPath);
    if (fs::exists(settingsPath)) {
        fs::remove(settingsPath);
    }
}

std::vector<std::string> ImportSettingsManager::GetAllAssetPaths() const {
    std::vector<std::string> paths;
    for (const auto& [path, _] : m_settings) {
        paths.push_back(path);
    }
    return paths;
}

void ImportSettingsManager::ApplyPresetToAll(ImportPreset preset, const std::string& typeFilter) {
    for (auto& [path, settings] : m_settings) {
        if (typeFilter.empty() || settings->GetTypeName() == typeFilter) {
            settings->ApplyPreset(preset);
        }
    }
}

std::string ImportSettingsManager::GetSettingsPath(const std::string& assetPath) {
    return assetPath + ".import";
}

std::string ImportSettingsManager::DetectAssetType(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Texture formats
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".bmp" || ext == ".dds" || ext == ".ktx" || ext == ".exr" ||
        ext == ".hdr" || ext == ".psd" || ext == ".gif") {
        return "Texture";
    }

    // Model formats
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" ||
        ext == ".dae" || ext == ".3ds" || ext == ".blend" || ext == ".stl") {
        return "Model";
    }

    // Animation formats
    if (ext == ".bvh" || ext == ".anim") {
        return "Animation";
    }

    return "Unknown";
}

void ImportSettingsManager::RegisterPreset(const std::string& name,
                                           std::function<void(ImportSettingsBase*)> applicator) {
    m_customPresets[name] = applicator;
}

// ============================================================================
// Utility Function Implementations
// ============================================================================

const char* GetCompressionName(TextureCompression compression) {
    switch (compression) {
        case TextureCompression::None: return "None";
        case TextureCompression::BC1: return "BC1 (DXT1)";
        case TextureCompression::BC3: return "BC3 (DXT5)";
        case TextureCompression::BC4: return "BC4 (ATI1)";
        case TextureCompression::BC5: return "BC5 (ATI2)";
        case TextureCompression::BC6H: return "BC6H (HDR)";
        case TextureCompression::BC7: return "BC7";
        case TextureCompression::ETC1: return "ETC1";
        case TextureCompression::ETC2_RGB: return "ETC2 RGB";
        case TextureCompression::ETC2_RGBA: return "ETC2 RGBA";
        case TextureCompression::ASTC_4x4: return "ASTC 4x4";
        case TextureCompression::ASTC_6x6: return "ASTC 6x6";
        case TextureCompression::ASTC_8x8: return "ASTC 8x8";
        case TextureCompression::PVRTC_RGB: return "PVRTC RGB";
        case TextureCompression::PVRTC_RGBA: return "PVRTC RGBA";
        default: return "Unknown";
    }
}

float GetCompressionBPP(TextureCompression compression) {
    switch (compression) {
        case TextureCompression::None: return 32.0f;  // RGBA8
        case TextureCompression::BC1: return 4.0f;
        case TextureCompression::BC3: return 8.0f;
        case TextureCompression::BC4: return 4.0f;
        case TextureCompression::BC5: return 8.0f;
        case TextureCompression::BC6H: return 8.0f;
        case TextureCompression::BC7: return 8.0f;
        case TextureCompression::ETC1: return 4.0f;
        case TextureCompression::ETC2_RGB: return 4.0f;
        case TextureCompression::ETC2_RGBA: return 8.0f;
        case TextureCompression::ASTC_4x4: return 8.0f;
        case TextureCompression::ASTC_6x6: return 3.56f;
        case TextureCompression::ASTC_8x8: return 2.0f;
        case TextureCompression::PVRTC_RGB: return 4.0f;
        case TextureCompression::PVRTC_RGBA: return 4.0f;
        default: return 32.0f;
    }
}

bool CompressionSupportsAlpha(TextureCompression compression) {
    switch (compression) {
        case TextureCompression::BC1:
        case TextureCompression::BC4:
        case TextureCompression::BC6H:
        case TextureCompression::ETC1:
        case TextureCompression::ETC2_RGB:
        case TextureCompression::PVRTC_RGB:
            return false;
        default:
            return true;
    }
}

TextureCompression GetPlatformCompression(TargetPlatform platform, bool hasAlpha, bool isNormalMap) {
    if (isNormalMap) {
        switch (platform) {
            case TargetPlatform::Desktop:
            case TargetPlatform::Console:
                return TextureCompression::BC5;
            case TargetPlatform::Mobile:
                return TextureCompression::ETC2_RGB;
            default:
                return TextureCompression::None;
        }
    }

    switch (platform) {
        case TargetPlatform::Desktop:
        case TargetPlatform::Console:
            return hasAlpha ? TextureCompression::BC7 : TextureCompression::BC1;

        case TargetPlatform::Mobile:
            return hasAlpha ? TextureCompression::ASTC_4x4 : TextureCompression::ETC2_RGB;

        case TargetPlatform::WebGL:
            return TextureCompression::None;

        default:
            return TextureCompression::None;
    }
}

} // namespace Nova
