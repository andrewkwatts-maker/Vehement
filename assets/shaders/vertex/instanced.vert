#version 330 core

// Standard vertex attributes (per-vertex)
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec3 a_Tangent;

// Instance attributes (per-instance)
layout (location = 4) in mat4 a_InstanceModel;      // Takes locations 4-7
layout (location = 8) in mat4 a_InstanceNormalMat;  // Takes locations 8-11
layout (location = 12) in vec4 a_InstanceColor;
layout (location = 13) in uint a_InstanceID;

// Uniforms
uniform mat4 u_ProjectionView;
uniform vec3 u_CameraPosition;
uniform float u_Time;

// Output to fragment shader
out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 InstanceColor;
    flat uint InstanceID;
    vec3 ViewDir;
} vs_out;

void main() {
    // Transform position to world space
    vec4 worldPos = a_InstanceModel * vec4(a_Position, 1.0);
    vs_out.FragPos = worldPos.xyz;

    // Transform normal using normal matrix
    mat3 normalMat = mat3(a_InstanceNormalMat);
    vs_out.Normal = normalize(normalMat * a_Normal);
    vs_out.Tangent = normalize(normalMat * a_Tangent);
    vs_out.Bitangent = cross(vs_out.Normal, vs_out.Tangent);

    // Pass through texture coordinates
    vs_out.TexCoord = a_TexCoord;

    // Instance data
    vs_out.InstanceColor = a_InstanceColor;
    vs_out.InstanceID = a_InstanceID;

    // View direction
    vs_out.ViewDir = normalize(u_CameraPosition - vs_out.FragPos);

    // Final position
    gl_Position = u_ProjectionView * worldPos;
}
