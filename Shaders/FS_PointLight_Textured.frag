#version 410
in vec4 vNormal;
in vec4 vPosition;
in vec2 vTexCoord;
out vec4 FragColor;
uniform vec3 LightPos;
uniform vec3 LightColour;
uniform vec3 CameraPos;
uniform float SpecPower;
uniform float Brightness;
uniform sampler2D diffuse;

float Intensity()
{
return Brightness/length(LightPos-vPosition.xyz);
}

float Spec()
{
vec3 E = normalize(CameraPos-vPosition.xyz);
vec3 R = reflect(-(LightPos-vPosition.xyz),vNormal.xyz);
float s = max(0,dot(E,R)*Intensity());
return pow(s,SpecPower);
}

void main()
{
float D = max(0,dot(normalize(vNormal.xyz),normalize(LightPos-vPosition.xyz)));
vec3 TexCol = texture(diffuse,vTexCoord).xyz;
vec3 Col = Intensity()*(D*TexCol+Spec()*TexCol)*LightColour;
FragColor = vec4(Col,1);
};