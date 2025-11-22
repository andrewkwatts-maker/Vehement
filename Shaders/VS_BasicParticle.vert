#version 410
layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Colour;

out vec4 colour;
uniform mat4 ProjectionView;

void main()
{
colour = Colour;
gl_Position = ProjectionView*Position;
};