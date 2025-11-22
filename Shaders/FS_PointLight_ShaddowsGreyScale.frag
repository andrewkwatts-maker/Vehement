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
uniform float Brightness;

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

void main()
{
vec3 Col = Intensity()*LightColour;
FragColor = vec4(Col,1);
};