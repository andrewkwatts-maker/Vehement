#version 410
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
out vec2 vTexCoord;
uniform mat4 ProjectionView;
uniform mat4 Transform;
void main()
{
vTexCoord = TexCoord;
gl_Position = ProjectionView*Transform*Position;
};