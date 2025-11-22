#version 410

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texcoords;
layout(location = 4) in vec4 weights;
layout(location = 5) in vec4 indices;

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBiTangent;
out vec3 vPosition;


uniform mat4 ProjectionView;
uniform mat4 Transform;

const int MAX_BONES = 128;
uniform mat4 bones[MAX_BONES];

void main()
{

vTexCoord = texcoords;
vNormal = normal.xyz;
vTangent = tangent.xyz;
vBiTangent = cross(vNormal,vTangent);

ivec4 index = ivec4(indices);

vec4 P =bones[index.x]*position*weights.x;
P +=bones[index.y]*position*weights.y;
P +=bones[index.z]*position*weights.z;
P +=bones[index.w]*position*weights.w;

vPosition = (Transform*P).xyz;
gl_Position = ProjectionView*Transform*P;

};