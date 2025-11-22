#version 410
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBiTangent;
in vec3 vPosition;
out vec4 FragColor;
uniform vec3 LightPos;
uniform vec3 LightColour;
uniform vec3 CameraPos;
uniform float SpecPower;
uniform float SpecIntensity;
uniform float Brightness;

uniform sampler2D diffuse;
uniform sampler2D normal;
uniform sampler2D specular;



float Intensity()
{
return Brightness/length(LightPos-vPosition.xyz);
}

vec3 GetNormal()
{
mat3 TBN = mat3(normalize(vTangent),normalize(vBiTangent),normalize(vNormal));
vec3 N = texture(normal,vTexCoord).xyz *2-1;
return normalize(TBN*N);
}


float Spec(vec3 Norm)
{
vec3 E = normalize(CameraPos-vPosition.xyz);
vec3 R = reflect(-normalize(LightPos-vPosition.xyz),Norm);
float s = max(0,dot(E,R)*Intensity());
return pow(s,SpecPower);
}

void main()
{
vec3 Normal = GetNormal();
float D = max(0,dot(Normal,normalize(LightPos-vPosition.xyz)));

vec3 TexCol = texture(diffuse,vTexCoord).xyz;
vec3 Col = Intensity()*(D*TexCol+SpecIntensity*Spec(Normal)*TexCol*texture(specular,vTexCoord).xyz)*LightColour;
FragColor = vec4(Col,1);
};



//float Spec()
//{
//vec3 E = normalize(CameraPos-vPosition.xyz);
//vec3 R = reflect(-(LightPos-vPosition.xyz),vNormal.xyz);
//float s = max(0,dot(E,R)*Intensity());
//return pow(s,SpecPower);
//}

//void main()
//{
//mat3 TBN = mat3(normalize(vTangent),normalize(vBiTangent),normalize(vNormal));
//vec3 N = texture(normal,vTexCoord).xyz *2-1;
//float D = max(0,dot(normalize(TBN*N),normalize(LightPos-vPosition.xyz)));
//float D = max(0,dot(normalize(vNormal.xyz),normalize(LightPos-vPosition.xyz)));

//vec3 TexCol = texture(diffuse,vTexCoord).xyz;
//vec3 Col = Intensity()*(D*TexCol+Spec()*TexCol)*LightColour;
//FragColor = vec4(Col,1);
//};