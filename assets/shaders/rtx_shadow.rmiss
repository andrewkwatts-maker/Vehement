#version 460
#extension GL_NV_ray_tracing : require

/**
 * @file rtx_shadow.rmiss
 * @brief Shadow Ray Miss Shader
 *
 * Executed when a shadow ray doesn't hit any geometry.
 * Indicates the point is not in shadow.
 */

// Shadow ray payload
layout(location = 1) rayPayloadInNV bool shadowed;

void main() {
    // Ray didn't hit anything, so not in shadow
    shadowed = false;
}
