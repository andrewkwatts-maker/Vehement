#version 430 core

out vec4 FragColor;

in vec4 v_fragPosLightSpace;

// Variance Shadow Map output
// R = depth, G = depth^2
void main() {
    float depth = v_fragPosLightSpace.z / v_fragPosLightSpace.w;
    depth = depth * 0.5 + 0.5; // Transform to [0, 1]

    float moment1 = depth;
    float moment2 = depth * depth;

    // Adjusting for partial derivatives to reduce bias
    float dx = dFdx(depth);
    float dy = dFdy(depth);
    moment2 += 0.25 * (dx * dx + dy * dy);

    FragColor = vec4(moment1, moment2, 0.0, 1.0);
}
