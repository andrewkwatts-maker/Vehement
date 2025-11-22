#version 410
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 Normal;
layout(location = 3) in vec4 Tangent;

out vec3 vPosition;
uniform mat4 ProjectionView;
uniform mat4 Transform;
void main()
{
vPosition = (Transform*Position).xyz;
gl_Position = ProjectionView*Transform*Position;
};