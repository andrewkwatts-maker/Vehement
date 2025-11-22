#version 410

in vec2	vTexCoord;
in vec4 pixelLoc;
in vec3 surfaceNormal;
in vec3 surfaceTangent;

out vec4 FragColor;

uniform sampler2D diffuse;
uniform sampler2D normal;
uniform sampler2D heightmap;
uniform sampler2D roughness;
uniform sampler2D diffuse2;
uniform sampler2D heightmap2;

uniform vec3 camloc;
uniform float depth_step_shift;

uniform float T1Height;
uniform float T2Height;
uniform float T1HScale;
uniform float T2HScale;
uniform float LerpPow;


vec3 Dir2Pixel;
vec3 inwardNormal;
float inwardDot;

vec2 UV_step_shift;

vec2 Current_UV;
vec2 Last_UV;

float CurrentDepth;
float Lerp;

void main()
{
inwardNormal = -surfaceNormal;
	Dir2Pixel = normalize(pixelLoc.xyz - camloc);
	inwardDot = dot(inwardNormal,Dir2Pixel);

	vec3 Vvec;

	float Udot;
	float Vdot;

	if(inwardDot>0)
	{

		Vvec = cross(surfaceTangent,surfaceNormal);
		Udot = dot(surfaceTangent,Dir2Pixel);
		Vdot = dot(Vvec,Dir2Pixel);

		bool AboveHeightmap = true;
		float Step = 0;
		float MaxSteps = 100;
	
		float LastDeltaDepth = 0.0f;
		float ThisDeltaDepth;
		float LastDeltaDepth2 = 0.0f;
		float ThisDeltaDepth2;

		float InvDot = 1/inwardDot;
		UV_step_shift = vec2(InvDot*Udot*depth_step_shift,InvDot*Vdot*depth_step_shift);
		vec3 AdjustmentVector = InvDot*Dir2Pixel*depth_step_shift;
		Current_UV = vTexCoord;
		Lerp = 1;
		while(AboveHeightmap && Step < MaxSteps)
		{
			Current_UV = UV_step_shift*Step+vTexCoord;
			CurrentDepth = depth_step_shift*Step;
			ThisDeltaDepth = T1Height+texture(heightmap,Current_UV).x*T1HScale- (1-CurrentDepth*10);
			if(ThisDeltaDepth>0)
			{
				if(Step>0)
				Lerp = LastDeltaDepth/(LastDeltaDepth-ThisDeltaDepth);
				
				AboveHeightmap = false;
			}
			ThisDeltaDepth2 = T2Height+texture(heightmap2,Current_UV).x*T2HScale - (1-CurrentDepth*10);
			if(ThisDeltaDepth2>0)
			{
				if(Step>0)
				Lerp = LastDeltaDepth2/(LastDeltaDepth2-ThisDeltaDepth2);
				AboveHeightmap = false;
			}


			Step = Step+1;
			LastDeltaDepth = ThisDeltaDepth;
			LastDeltaDepth2 = ThisDeltaDepth2;
		}
		Lerp = pow(Lerp,LerpPow);
		Last_UV = UV_step_shift*(Step-2)+vTexCoord;

		if(ThisDeltaDepth2>0)
		{	
		FragColor = texture(diffuse2,(Lerp)*Current_UV+(1-Lerp)*Last_UV);
		}
		else
		{
		FragColor = texture(diffuse,(Lerp)*Current_UV+(1-Lerp)*Last_UV);
		}


	}
	else
	{
		discard;
	}
};

		//if(waterHeight>1-CurrentDepth*10)
		//	FragColor = texture(diffuse2,Current_UV);
		//else
		//	FragColor = texture(diffuse,Current_UV);
