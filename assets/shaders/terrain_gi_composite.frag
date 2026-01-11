#version 430 core

// Final composite shader for terrain rendering
// Combines rasterized primary visibility with raytraced GI

out vec4 FragColor;

in vec2 v_texCoord;

// Primary pass
uniform sampler2D u_primaryColor;       // Albedo + roughness
uniform sampler2D u_primaryNormal;      // Normal + metallic
uniform sampler2D u_primaryDepth;       // Depth

// Secondary pass (GI)
uniform sampler2D u_giResult;           // GI lighting

// Previous frame (temporal accumulation)
uniform sampler2D u_prevFrame;
uniform bool u_useTemporalAccumulation;
uniform float u_temporalBlend;          // 0.95 = strong accumulation

// Camera
uniform mat4 u_viewProj;
uniform mat4 u_prevViewProj;
uniform vec3 u_cameraPos;

// Post-processing
uniform bool u_enableToneMapping;
uniform bool u_enableGammaCorrection;
uniform float u_exposure;
uniform int u_frameIndex;

// =============================================================================
// Utility
// =============================================================================

vec3 reconstructWorldPos(vec2 uv, float depth, mat4 invViewProj) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

// =============================================================================
// Temporal Reprojection
// =============================================================================

vec3 temporalReproject(vec2 uv, float depth, out vec2 prevUV, out bool valid) {
    valid = false;

    // Reconstruct world position
    mat4 invViewProj = inverse(u_viewProj);
    vec3 worldPos = reconstructWorldPos(uv, depth, invViewProj);

    // Project to previous frame
    vec4 prevClip = u_prevViewProj * vec4(worldPos, 1.0);
    prevClip.xyz /= prevClip.w;
    prevUV = prevClip.xy * 0.5 + 0.5;

    // Check if in screen bounds
    if (any(lessThan(prevUV, vec2(0))) || any(greaterThan(prevUV, vec2(1)))) {
        return vec3(0.0);
    }

    // Check depth similarity (reject disocclusions)
    float prevDepth = texture(u_primaryDepth, prevUV).r;
    float depthDiff = abs(prevDepth - depth);

    if (depthDiff > 0.001) {
        return vec3(0.0);  // Disoccluded
    }

    valid = true;
    return texture(u_prevFrame, prevUV).rgb;
}

// =============================================================================
// Tone Mapping
// =============================================================================

vec3 reinhardToneMapping(vec3 color, float exposure) {
    color *= exposure;
    return color / (1.0 + color);
}

vec3 acesToneMapping(vec3 color, float exposure) {
    color *= exposure;

    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// =============================================================================
// Main
// =============================================================================

void main() {
    vec2 uv = v_texCoord;

    // Sample primary pass
    vec4 colorRoughness = texture(u_primaryColor, uv);
    vec4 normalMetallic = texture(u_primaryNormal, uv);
    float depth = texture(u_primaryDepth, uv).r;

    // Check for sky
    if (depth >= 1.0) {
        // Sky - could render skybox here
        FragColor = vec4(0.5, 0.7, 1.0, 1.0);  // Simple blue sky
        return;
    }

    vec3 albedo = colorRoughness.rgb;
    float roughness = colorRoughness.a;

    // Sample GI result
    vec3 giLighting = texture(u_giResult, uv).rgb;

    // Temporal accumulation to reduce noise
    vec3 finalColor = giLighting;

    if (u_useTemporalAccumulation && u_frameIndex > 0) {
        vec2 prevUV;
        bool valid;
        vec3 prevColor = temporalReproject(uv, depth, prevUV, valid);

        if (valid) {
            // Exponential moving average
            finalColor = mix(giLighting, prevColor, u_temporalBlend);

            // Clamp to avoid ghosting (simple variance clip)
            vec3 minColor = giLighting * 0.5;
            vec3 maxColor = giLighting * 1.5;
            finalColor = clamp(finalColor, minColor, maxColor);
        }
    }

    // Tone mapping
    if (u_enableToneMapping) {
        finalColor = acesToneMapping(finalColor, u_exposure);
    }

    // Gamma correction
    if (u_enableGammaCorrection) {
        finalColor = pow(finalColor, vec3(1.0 / 2.2));
    }

    FragColor = vec4(finalColor, 1.0);
}
