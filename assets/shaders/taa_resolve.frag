#version 460 core

out vec4 FragColor;

in vec2 v_texCoord;

// Input textures
uniform sampler2D u_currentFrame;
uniform sampler2D u_historyFrame;
uniform sampler2D u_velocityTexture;
uniform sampler2D u_depthTexture;

// Uniforms
uniform vec2 u_texelSize;
uniform float u_temporalAlpha;
uniform bool u_neighborhoodClamping;
uniform bool u_varianceClipping;
uniform bool u_useYCoCg;
uniform float u_velocityThreshold;

// RGB to YCoCg
vec3 RGBToYCoCg(vec3 rgb) {
    float Y = rgb.r * 0.25 + rgb.g * 0.5 + rgb.b * 0.25;
    float Co = rgb.r * 0.5 - rgb.b * 0.5;
    float Cg = -rgb.r * 0.25 + rgb.g * 0.5 - rgb.b * 0.25;
    return vec3(Y, Co, Cg);
}

// YCoCg to RGB
vec3 YCoCgToRGB(vec3 ycocg) {
    float Y = ycocg.x;
    float Co = ycocg.y;
    float Cg = ycocg.z;
    float r = Y + Co - Cg;
    float g = Y + Cg;
    float b = Y - Co - Cg;
    return vec3(r, g, b);
}

// Sample 3x3 neighborhood
vec3[9] sampleNeighborhood(sampler2D tex, vec2 uv) {
    vec3 samples[9];
    int index = 0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offset = vec2(x, y) * u_texelSize;
            samples[index++] = texture(tex, uv + offset).rgb;
        }
    }
    return samples;
}

// Calculate neighborhood AABB (min/max)
void calculateNeighborhoodAABB(vec3[9] samples, out vec3 aabbMin, out vec3 aabbMax) {
    aabbMin = samples[4]; // Center
    aabbMax = samples[4];

    for (int i = 0; i < 9; i++) {
        aabbMin = min(aabbMin, samples[i]);
        aabbMax = max(aabbMax, samples[i]);
    }
}

// Calculate neighborhood mean and variance
void calculateNeighborhoodStats(vec3[9] samples, out vec3 mean, out vec3 variance) {
    mean = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        mean += samples[i];
    }
    mean /= 9.0;

    variance = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        vec3 diff = samples[i] - mean;
        variance += diff * diff;
    }
    variance /= 9.0;
}

// Clip history color to neighborhood
vec3 clipToNeighborhood(vec3 historyColor, vec3[9] samples) {
    if (u_varianceClipping) {
        // Variance clipping
        vec3 mean, variance;
        calculateNeighborhoodStats(samples, mean, variance);

        vec3 stdDev = sqrt(variance);
        vec3 aabbMin = mean - stdDev * 1.5;
        vec3 aabbMax = mean + stdDev * 1.5;

        return clamp(historyColor, aabbMin, aabbMax);
    } else if (u_neighborhoodClamping) {
        // Simple AABB clamping
        vec3 aabbMin, aabbMax;
        calculateNeighborhoodAABB(samples, aabbMin, aabbMax);
        return clamp(historyColor, aabbMin, aabbMax);
    }

    return historyColor;
}

void main() {
    vec2 uv = v_texCoord;

    // Sample current frame
    vec3 currentColor = texture(u_currentFrame, uv).rgb;

    // Sample velocity
    vec2 velocity = texture(u_velocityTexture, uv).xy;
    vec2 historyUV = uv - velocity;

    // Check if history is valid
    bool validHistory = historyUV.x >= 0.0 && historyUV.x <= 1.0 &&
                       historyUV.y >= 0.0 && historyUV.y <= 1.0;

    if (!validHistory || length(velocity) > u_velocityThreshold) {
        // No valid history, use current frame
        FragColor = vec4(currentColor, 1.0);
        return;
    }

    // Sample history
    vec3 historyColor = texture(u_historyFrame, historyUV).rgb;

    // Convert to YCoCg if enabled
    if (u_useYCoCg) {
        currentColor = RGBToYCoCg(currentColor);
        historyColor = RGBToYCoCg(historyColor);
    }

    // Sample neighborhood
    vec3[9] neighborhood = sampleNeighborhood(u_currentFrame, uv);
    if (u_useYCoCg) {
        for (int i = 0; i < 9; i++) {
            neighborhood[i] = RGBToYCoCg(neighborhood[i]);
        }
    }

    // Clip history to neighborhood
    historyColor = clipToNeighborhood(historyColor, neighborhood);

    // Temporal blend
    float alpha = u_temporalAlpha;

    // Increase alpha for high-velocity regions (reduce ghosting)
    alpha = mix(alpha, 1.0, smoothstep(0.0, u_velocityThreshold, length(velocity)));

    vec3 finalColor = mix(historyColor, currentColor, alpha);

    // Convert back from YCoCg
    if (u_useYCoCg) {
        finalColor = YCoCgToRGB(finalColor);
    }

    FragColor = vec4(finalColor, 1.0);
}
