#pragma once
#include "Application.h"
#include "GA_OldBuilding.h"
#include "GA_BuildingCluster.h"

#include "GA_TerrianMap.h"
#include "AntTweak\AntTweakBar.h"
#include "GPU_ParticleEmitter.h"
class GraphicsAssigment :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	int NumberUniqueBuildings;
	float Age;
	float LightRadius;
	float Brightness;
	vec3 LightColour;
	bool AutoShift;
	float Frequency;
	float Amp;
	bool Raining;

	float OribtRadius;
	float ModelSize;
	int NumberModels;
	int ShowModel;


	float AgeInDecades;
	float DistanceToLight;
	float WaterLevel;
	float BuildLevel;
	float Settlement;

	bool ShowParticles;
	bool ShowPointAtLight;
	bool ShowCubeMap;

	vec3 LightPos;
	vec3 LookPos;
	vec3 CameraPos;

	float LastAge;
	int LastBuildings;
	int LastLightRadius;
	int FractureCount;
	int LastFractureCount;



	GraphicsAssigment();
	~GraphicsAssigment();

private:
	int RandomSeed = 1;
	std::vector<GA_BuildingCluster*> Buildings;
	std::vector<GA_BuildingCluster*> WholeBuildings;
	GA_TerrianMap* Map;
	
	TwBar* m_bar;

	GPU_ParticleEmitter* Emmitter;
	GPU_ParticleEmitter* RainEmmitter;
	float Time;

	void RenderXP(vec3 LightPos, float LightRadius);
	void RenderXN(vec3 LightPos, float LightRadius);
	void RenderYP(vec3 LightPos, float LightRadius);
	void RenderYN(vec3 LightPos, float LightRadius);
	void RenderZP(vec3 LightPos, float LightRadius);
	void RenderZN(vec3 LightPos, float LightRadius);

	void RenderScene(vec3 CameraPos,vec3 LightPos, float LightRadius);

	void RenderComponants(vec3 LightPos,float LightRadius,bool Finalpass);

	int CubeRender[6];


	int FrameBufferXP;
	int FrameBufferXN;
	int FrameBufferYP;
	int FrameBufferYN;
	int FrameBufferZP;
	int FrameBufferZN;

	int RenderTargetXP;
	int RenderTargetXN;
	int RenderTargetYP;
	int RenderTargetYN;
	int RenderTargetZP;
	int RenderTargetZN;




	int ShaddowsPointTextureBump;
	int ShaddowsGrey;
	int PointTexturedBump;
	int PointDepth;
	int RockDiffuse;
	int RockNormal;

	int SandDiffuse;
	int SandNormal;

	int WoodDiffuse;
	int WoodNormal;
	int WaterDiffuse;
	int WaterNormal;

	int SmokeDiffuse;
	int RainDiffuse;

	int SpearModel;
	int SpearDiffuse;
	int BunnyModel;
	


	int Water; //Geometry;
	int WaterShader;
	int Screen;
	int ScreenRenderProgram;

};

