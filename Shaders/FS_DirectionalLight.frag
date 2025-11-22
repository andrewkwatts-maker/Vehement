#version 410  
in vec4 vNormal;
in vec4 vPosition;
out vec4 FragColor;
uniform vec3 LightDir;
uniform vec3 LightColour;
uniform vec3 CameraPos;
uniform float SpecPower;
uniform float Brightness;

float SpecLight()
{
vec3 E = normalize(CameraPos - vPosition.xyz);
vec3 R = reflect(-LightDir,vNormal.xyz);
float s = max(0,dot(E,R));
s = Brightness*pow(s,SpecPower);
return s;
}

float DirLight()
{
return max(0,Brightness*dot(normalize(vNormal.xyz),normalize(LightDir)));
}

void main() 
{
FragColor = vec4(DirLight()*LightColour +SpecLight()*LightColour, 1);
};