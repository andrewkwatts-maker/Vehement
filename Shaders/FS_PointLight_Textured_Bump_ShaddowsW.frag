#version 410

//Inputs
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBiTangent;
in vec3 vPosition;

//Outputs
out vec4 FragColor;

//Uniforms
uniform vec3 LightPos;
uniform vec3 LightColour;
uniform vec3 CameraPos;
uniform float SpecPower;
uniform float SpecIntensity;
uniform float Brightness;

uniform sampler2D diffuse;
uniform sampler2D normal;

uniform float ADisplacment;
uniform float BDisplacment;

uniform float LightRadius;
uniform float BaseLight;
uniform sampler2D XP;
uniform sampler2D XN;
uniform sampler2D YP;
uniform sampler2D YN;
uniform sampler2D ZP;
uniform sampler2D ZN;




float Intensity()
{
	
	float d = length(LightPos-vPosition);	
	float v = max(1-d/LightRadius,0);
	if(v==0)
	{
		return BaseLight;
	}
	else
	{
		vec3 DeltaN = normalize(vPosition-LightPos);
		vec3 DeltaNA = abs(DeltaN);
		float Max = max(DeltaNA.x,max(DeltaNA.y,DeltaNA.z));

		vec3 MultDelt = (DeltaN/Max);
		float DistanceDelta = length(MultDelt/2) - 0.5f;

		float ref = v+0.01f;

		if(DeltaNA.x == Max)
		{
			vec2 Access = vec2(MultDelt.z,MultDelt.y);
			

			if(DeltaN.x > 0)
			{
				

				if(ref>texture(XP,Access/2 + vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
			else
			{
				Access.x = -Access.x;
				if(ref>texture(XN,Access/2 + vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
		}
		else if(DeltaNA.z == Max)
		{
			vec2 Access = vec2(-MultDelt.x,MultDelt.y);
			if(DeltaN.z > 0)
			{
				if(ref>texture(ZP,Access/2+vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
			else
			{
				Access.x =-Access.x;
				if(ref>texture(ZN,Access/2+vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
		}
		else
		{
			vec2 Access = vec2(-MultDelt.z,MultDelt.x);
			if(DeltaN.y > 0)
			{
				if(ref>texture(YP,Access/2 +vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
			else
			{
				Access.y = -Access.y;
				if(ref>texture(YN,Access/2 + vec2(0.5f)).x)
					return (Brightness*v)*(1-BaseLight)+BaseLight;
				else
					return BaseLight;
			}
		}
	}
}


vec2 GetATex()
{
	return vTexCoord + vec2(0.1f,0.3f)*ADisplacment;
}


vec2 GetBTex()
{
	return vTexCoord + vec2(-0.7f,-0.1f)*BDisplacment;
}

vec4 GetNormalTex()
{
	return texture(normal,GetATex())*0.5f + texture(normal,GetBTex())*0.5f;
}

vec4 GetDiffuseTex()
{
	return texture(diffuse,GetATex())*0.5f + texture(diffuse,GetBTex())*0.5f;
}


vec4 GetColour()
{
	float d = length(LightPos-vPosition);	
	float v = max(1-d/LightRadius,0);
	if(v==0)
	{
		return vec4(0,0,0,1);
	}
	else
	{
		vec3 DeltaN = normalize(vPosition-LightPos);
		vec3 DeltaNA = abs(DeltaN);
		float Max = max(DeltaNA.x,max(DeltaNA.y,DeltaNA.z));

		vec3 MultDelt = (DeltaN/Max);
		float DistanceDelta = length(MultDelt/2) - 0.5f;

		float ref = v+0.05f;

		if(DeltaNA.x == Max)
		{
			vec2 Access = vec2(MultDelt.z,MultDelt.y);
			

			if(DeltaN.x > 0)
			{
				

				if(ref>texture(XP,Access/2 + vec2(0.5f)).x)
					return texture(XP,Access/2 + vec2(0.5f));
				else
					return texture(XP,Access/2 + vec2(0.5f));
			}
			else
			{
				Access.x = -Access.x;
				if(ref>texture(XN,Access/2 + vec2(0.5f)).x)
					return texture(XN,Access/2 + vec2(0.5f));
				else
					return texture(XN,Access/2 + vec2(0.5f));
			}
		}
		else if(DeltaNA.z == Max)
		{
			vec2 Access = vec2(-MultDelt.x,MultDelt.y);
			if(DeltaN.z > 0)
			{
				if(ref>texture(ZP,Access/2+vec2(0.5f)).x)
					return texture(ZP,Access/2+vec2(0.5f));
				else
					return texture(ZP,Access/2+vec2(0.5f));
			}
			else
			{
				Access.x =-Access.x;
				if(ref>texture(ZN,Access/2+vec2(0.5f)).x)
					return texture(ZN,Access/2+vec2(0.5f));
				else
					return texture(ZN,Access/2+vec2(0.5f));
			}
		}
		else
		{
			vec2 Access = vec2(MultDelt.z,MultDelt.x);
			if(DeltaN.y > 0)
			{
				if(ref>texture(YP,Access/2 +vec2(0.5f)).x)
					return texture(YP,Access/2 +vec2(0.5f));
				else
					return texture(YP,Access/2 +vec2(0.5f));
			}
			else
			{
				Access.y = -Access.y;
				if(ref>texture(YN,Access/2 + vec2(0.5f)).x)
					return texture(YN,Access/2 + vec2(0.5f));
				else
					return texture(YN,Access/2 + vec2(0.5f));
			}
		}
	}
}

vec3 GetNormal()
{
mat3 TBN = mat3(normalize(vTangent),normalize(vBiTangent),normalize(vNormal));
vec3 N = GetNormalTex().xyz *2-1;
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
float spe = SpecIntensity*Spec(Normal);
vec3 TexCol = GetDiffuseTex().xyz;
vec3 Col = Intensity()*(D*TexCol+spe*TexCol)*LightColour;
FragColor = vec4(Col,max(spe,GetDiffuseTex().a/2));
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