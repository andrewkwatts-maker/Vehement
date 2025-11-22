#version 410
layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;
out vec4 vNormal;
out vec4 vPosition;
uniform mat4 ProjectionView;
uniform mat4 Transform;
void main()
{
vNormal = Normal;
vPosition = Transform*Position;
gl_Position = ProjectionView*Transform*Position;
};