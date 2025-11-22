#version 410
layout(location = 0) in vec4 Position;
layout(location = 2) in vec4 Normal;
layout(location = 1) in vec2 TexCoord;
layout(location = 3) in vec4 Tangent;

//Find out why i need to jumble locations

out vec2 vTexCoord;
out vec4 pixelLoc;
out vec3 surfaceNormal;
out vec3 surfaceTangent;

uniform mat4 ProjectionView;
uniform mat4 Transform;

void main()
{
surfaceNormal = normalize(Normal.xyz);
surfaceTangent = normalize(Tangent.xyz);
vTexCoord = TexCoord;
pixelLoc = Transform*Position;
gl_Position = ProjectionView*Transform*Position;
};