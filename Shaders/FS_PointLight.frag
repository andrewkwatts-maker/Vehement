#version 410
in vec4 vNormal;
in vec4 vPosition;
out vec4 FragColor;
uniform vec3 LightPos;
uniform vec3 LightColour;
uniform vec3 CameraPos;
uniform float SpecPower;
uniform float Brightness;

float Intensity()
{
return Brightness/length(LightPos-vPosition.xyz);
}

float Spec()
{
vec3 E = normalize(CameraPos-vPosition.xyz);
vec3 R = reflect(-normalize(LightPos-vPosition.xyz),vNormal.xyz);
float s = max(0,dot(E,R));
return pow(s,SpecPower);
}

void main()
{
float D = max(0,dot(normalize(vNormal.xyz),normalize(LightPos-vPosition.xyz)));
FragColor = Intensity()*vec4(D*LightColour+Spec()*LightColour,1);
};