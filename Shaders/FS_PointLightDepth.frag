#version 410
in vec3 vPosition;

out vec4 FragColor;
uniform vec3 LightPos;
uniform float LightRadius;

void main()
{
float D = length(LightPos-vPosition); //Depth
FragColor = vec4(1-D/LightRadius,1-D/LightRadius,1-D/LightRadius,1); //Results
};