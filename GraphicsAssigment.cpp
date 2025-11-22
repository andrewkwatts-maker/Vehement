#include "GraphicsAssigment.h"
#include "GL_Manager.h"
#include "Camera.h"
#include "Clock.h"
#include "glm\glm.hpp"
#include "glm\ext.hpp"
#include "Inputs.h"

GraphicsAssigment::GraphicsAssigment()
{
	LightRadius = 37;
	Brightness = 1;
	LightColour = vec3(1);
	AutoShift = true;
	ShowCubeMap = false;
	Age = 0.0f;
	AgeInDecades = Age * 40;
	LastAge = -1.0f;
	NumberUniqueBuildings = 22;
	Frequency = 67;
	WaterLevel = 0;
	BuildLevel = 0;
	Amp = 27;
	DistanceToLight = 10.0f;
	FractureCount = 12;
	LastFractureCount = 12;
	ShowParticles = false;
	ShowPointAtLight = false;
	Settlement = 50;
	Raining = true;


	OribtRadius = 1.2f;
	ModelSize = 0.25f; 
	NumberModels = 7;
	ShowModel = 0;

	CameraPos = vec3(-75.62f, 13.53f, -59.52f);
	LightPos = vec3(-69.62f, 12.59f, -51.58f);
	

}


GraphicsAssigment::~GraphicsAssigment()
{
	TwDeleteAllBars();
	TwTerminate();
}


bool GraphicsAssigment::update()
{
	if (Application::update())
	{
		Time += appBasics->AppClock->GetDelta();

		printf("%f \n", Time);
			
		Map->AutoUpdate = AutoShift;
		Map->VissibleRange = 2*(LightRadius);
		Map->UpdateDeltaRequirment = 1;
		Map->UpdateMapSpace(LightPos,Frequency,Amp);

		while (Buildings.size() < NumberUniqueBuildings)
		{
			int r = Buildings.size();
			Buildings.push_back(new GA_BuildingCluster());
			WholeBuildings.push_back(new GA_BuildingCluster());
			srand(RandomSeed + r);
			Buildings[r]->Build(Age,FractureCount);
			srand(RandomSeed + r);
			WholeBuildings[r]->Build(Age, 2);
		}

		if (LastFractureCount != FractureCount)
		{
			if (FractureCount < 1)
				FractureCount = 1;
			for (int i = 0; i < Buildings.size(); ++i)
			{
				delete Buildings[i];
				delete WholeBuildings[i];
				Buildings.erase(Buildings.begin() + i);
				WholeBuildings.erase(WholeBuildings.begin() + i);
				--i;
			}

			for (int i = 0; i < NumberUniqueBuildings; ++i)
			{
				int r = Buildings.size();
				Buildings.push_back(new GA_BuildingCluster());
				WholeBuildings.push_back(new GA_BuildingCluster());

				srand(RandomSeed + i);
				Buildings[r]->Build(Age, FractureCount);
				srand(RandomSeed + i);
				WholeBuildings[r]->Build(Age, 2);
			}
			LastFractureCount = FractureCount;
			LastAge = -1;
		}


		if (AgeInDecades < 0)
			AgeInDecades = 0;

		if (Settlement < 0)
			Settlement = 0;
		if (Settlement > 100)
			Settlement = 100;

		Age = AgeInDecades / (101.0f-1*Settlement);

		if (LastAge != Age)
		{
			for (int i = 0; i < NumberUniqueBuildings; ++i)
			{
				Buildings[i]->ApplyAge(Age, 3, i, Buildings[i]->Details.x, Buildings[i]->Details.y);
			}
			LastAge = Age;
		}

		return true;
	}
	else
	{
		return false;
	}

}
void GraphicsAssigment::draw()
{
	if (DistanceToLight <= 0)
		DistanceToLight = 0;
	CameraPos = appBasics->AppCamera->GetPos();

	LookPos = appBasics->AppCamera->GetPos() + appBasics->AppCamera->GetDirVector()*(DistanceToLight + 0.01f);

	if (appBasics->AppInputs->IsKeyDown(GLFW_KEY_LEFT_SHIFT))
		LightPos = LookPos;

	if (NumberModels < 0)
		NumberModels = 0;
	if (ShowModel < 0)
		ShowModel = 0;
	if (ShowModel > 2)
		ShowModel = 2;

	RenderXN(LightPos, LightRadius);
	RenderXP(LightPos, LightRadius);
	RenderYN(LightPos, LightRadius);
	RenderYP(LightPos, LightRadius);
	RenderZN(LightPos, LightRadius);
	RenderZP(LightPos, LightRadius);


	OGL_Manager->BeginNewDrawTo(0, vec4(0, 0, 0, 1));

	if (!ShowCubeMap)
	{
		appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.25f, appBasics->ScreenSize.x / appBasics->ScreenSize.y, 0.1f, 1000.0f);
		appBasics->AppCamera->SetupCamera(CameraPos, LookPos, vec3(0, 1, 0));

		RenderScene(CameraPos, LightPos, LightRadius);
		
		


		//OGL_Manager->UseShader(PointTexturedBump);
		//OGL_Manager->PassInUniform("LightPos", LightPos);
		//OGL_Manager->PassInUniform("LightColour", LightColour);
		//OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
		//OGL_Manager->PassInUniform("SpecPower", 1.0f);
		//OGL_Manager->PassInUniform("SpecIntensity", 1.0f);
		//OGL_Manager->PassInUniform("Brightness", 14.5f);
		//OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
		////OGL_Manager->SetTexture(FernTex, 0, "diffuse");
		


		OGL_Manager->UseShader(WaterShader);
		OGL_Manager->PassInUniform("LightPos", LightPos);
		OGL_Manager->PassInUniform("LightColour", LightColour);
		OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
		OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
		OGL_Manager->PassInUniform("LightRadius", (float)LightRadius);
		OGL_Manager->PassInUniform("BaseLight", 0.0f);
		
		OGL_Manager->PassInUniform("ADisplacment", (float)appBasics->AppClock->GetProgramTime().Second/5);
		OGL_Manager->PassInUniform("BDisplacment", (float)appBasics->AppClock->GetProgramTime().Second/7 - 12);
		
		OGL_Manager->PassInUniform("Brightness", Brightness);
		OGL_Manager->PassInUniform("SpecIntensity", 1.2f);
		OGL_Manager->PassInUniform("SpecPower", 1.0f);

		OGL_Manager->SetTexture(WaterDiffuse, 0, "diffuse");
		OGL_Manager->SetTexture(WaterNormal, 1, "normal");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetXN, 2, "XN");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetXP, 3, "XP");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetYN, 4, "YN");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetYP, 5, "YP");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetZN, 6, "ZN");
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetZP, 7, "ZP");

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		OGL_Manager->DrawCustomGeometry(Water, vec3(0,WaterLevel, 0));
		glDisable(GL_BLEND);

		if (ShowParticles == true)
		{
			Emmitter->DrawAt(LightPos, Time, OGL_Manager, appBasics->AppCamera->GetWorldTransform(), appBasics->AppCamera->GetProjectionView(), SmokeDiffuse);
		}
		if (Raining == true)
		{
			RainEmmitter->DrawRainAt(vec3(LightPos + vec3(2 * LightRadius, 2 * Amp, 2 * LightRadius)), vec3(LightPos - vec3(2 * LightRadius, Amp, 2 * LightRadius)), Time, OGL_Manager, appBasics->AppCamera->GetWorldTransform(), appBasics->AppCamera->GetProjectionView(), RainDiffuse);
		}

		if (ShowPointAtLight == true)
		{
			Gizmos::addTransform(mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(LightPos, 1)));
		}
	}
	else
	{
		appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.25f, appBasics->ScreenSize.x / appBasics->ScreenSize.y, 0.1f, 1000.0f);
		appBasics->AppCamera->SetupCamera(CameraPos, LookPos, vec3(0, 1, 0));

		OGL_Manager->UseShader(ScreenRenderProgram);
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetXP, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[0], vec3(0, 0, 0));
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetZP, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[1], vec3(0, 0, 0));
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetXN, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[2], vec3(0, 0, 0));
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetZN, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[3], vec3(0, 0, 0));
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetYP, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[4], vec3(0, 0, 0));
		OGL_Manager->SetRenderTargetAsTexture(RenderTargetYN, 0, "diffuse");
		OGL_Manager->DrawCustomGeometry(CubeRender[5], vec3(0, 0, 0));
	}

	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
	TwDraw();
}
bool GraphicsAssigment::startup()
{
	if (Application::startup())
	{
		appBasics->AppCamera->SetupCamera(CameraPos, LookPos, vec3(0, 1, 0));

		//Menu
		m_bar = TwNewBar("RUINS - Andrew W");

		TwAddSeparator(m_bar, "Light", NULL);
		TwAddVarRW(m_bar, "LightRadius", TW_TYPE_FLOAT, &LightRadius, "");
		TwAddVarRW(m_bar, "Brightness", TW_TYPE_FLOAT, &Brightness, "");
		TwAddVarRW(m_bar, "Colour", TW_TYPE_COLOR3F, &LightColour, "");
		TwAddVarRW(m_bar, "Distance2Light", TW_TYPE_FLOAT, &DistanceToLight, "");
		TwAddVarRW(m_bar, "ShowCubeMap", TW_TYPE_BOOL8, &ShowCubeMap, "");
		//TwAddVarRW(m_bar, "Cam", TW_TYPE_DIR3F, &CameraPos, "");
		//TwAddVarRW(m_bar, "Light2", TW_TYPE_DIR3F, &LightPos, "");


		TwAddSeparator(m_bar, "Land", NULL);
		TwAddVarRW(m_bar, "LandSize", TW_TYPE_FLOAT, &Frequency, "");
		TwAddVarRW(m_bar, "LandAmplitude", TW_TYPE_FLOAT, &Amp, "");
		TwAddVarRW(m_bar, "ShiftTerrain", TW_TYPE_BOOL8, &AutoShift, "");
		TwAddVarRW(m_bar, "WaterLevel", TW_TYPE_FLOAT, &WaterLevel, "");
		TwAddVarRW(m_bar, "Raining", TW_TYPE_BOOL8, &Raining, "");

		TwAddSeparator(m_bar, "Buildings", NULL);
		TwAddVarRW(m_bar, "BuildAboveHeight", TW_TYPE_FLOAT, &BuildLevel, "");
		TwAddVarRW(m_bar, "UniqueBuildings", TW_TYPE_INT16, &NumberUniqueBuildings, "");
		TwAddVarRW(m_bar, "DecadesPast", TW_TYPE_FLOAT, &AgeInDecades, "");
		TwAddVarRW(m_bar, "WallSegments", TW_TYPE_INT16, &FractureCount, "");
		TwAddVarRW(m_bar, "SettlementRate", TW_TYPE_FLOAT, &Settlement, "");	

		TwAddSeparator(m_bar, "Others", NULL);
		TwAddVarRW(m_bar, "ShowLocLight", TW_TYPE_BOOL8, &ShowPointAtLight, "");
		TwAddVarRW(m_bar, "ShowParticles", TW_TYPE_BOOL8, &ShowParticles, "");
		TwAddVarRW(m_bar, "ShowModel", TW_TYPE_INT16, &ShowModel, "");
		TwAddVarRW(m_bar, "NumberModels", TW_TYPE_INT16, &NumberModels, "");
		TwAddVarRW(m_bar, "ModelSize", TW_TYPE_FLOAT, &ModelSize, "");
		TwAddVarRW(m_bar, "OrbitRadius", TW_TYPE_FLOAT, &OribtRadius, "");

		RunDrawBeginAndEnd = false;
		



		Map = new GA_TerrianMap(appBasics->AppCamera->GetPos());
		Map->AutoUpdate = AutoShift;
		Map->VissibleRange = 2*(LightRadius+1);
		Map->UpdateDeltaRequirment = 1;

		//PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump_Spec_Anim.vert", "./Shaders/FS_PointLight_Textured_Bump_Spec_Anim.frag");
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");
		ShaddowsPointTextureBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump_Shaddows.vert", "./Shaders/FS_PointLight_Textured_Bump_Shaddows.frag");
		WaterShader = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump_Shaddows.vert", "./Shaders/FS_PointLight_Textured_Bump_ShaddowsW.frag");
		ShaddowsGrey = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump_Shaddows.vert", "./Shaders/FS_PointLight_ShaddowsGreyScale.frag");
		PointDepth = OGL_Manager->AddShaders("./Shaders/VS_PointLightDepth.vert", "./Shaders/FS_PointLightDepth.frag");
		ScreenRenderProgram = OGL_Manager->AddShaders("./Shaders/VS_Textured_NoCam.vert", "./Shaders/FS_Textured_NoCam.frag");

		//CubeScreenRenderer
		CubeRender[0] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(-1.0f, -0.3f), glm::vec2(-0.5f, 0.3f), appBasics->ScreenSize);
		CubeRender[1] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(-0.5f, -0.3f), glm::vec2(0.0f, 0.3f), appBasics->ScreenSize);
		CubeRender[2] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(0.0f, -0.3f), glm::vec2(0.5f, 0.3f), appBasics->ScreenSize);
		CubeRender[3] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(0.5f, -0.3f), glm::vec2(1.0f, 0.3f), appBasics->ScreenSize);
		CubeRender[4] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(-0.0f, 0.3f), glm::vec2(0.5f, 0.9f), appBasics->ScreenSize);
		CubeRender[5] = OGL_Manager->AddscreenQuadGeometry(0, glm::vec2(-0.0f, -0.9f), glm::vec2(0.5f, -0.3f), appBasics->ScreenSize);



		//ParticleSystem
		Time = 0;
		const char* Varyiances[] = { "position", "velocity", "lifetime", "lifespan" };
		int UpdateShader = OGL_Manager->AddUpdateShader("./Shaders/VS_gpuParticleUpdate.vert", Varyiances, 4);
		int Shader = OGL_Manager->AddShaders("./Shaders/VS_gpuParticle.vert", "./Shaders/FS_gpuParticle.frag", "./Shaders/GS_gpuParticle.geom");
		int RainUpdate = OGL_Manager->AddUpdateShader("./Shaders/VS_gpuRainParticleUpdate.vert", Varyiances, 4);

		GPU_PE_Constructer Constructor;
		Constructor.m_endColour = glm::vec4(1, 1, 1, 0);
		Constructor.m_startColour = glm::vec4(1, 1, 1, 0.5f);
		Constructor.m_lifespanMin = 0.1f;
		Constructor.m_lifespanMax = 2.0f;
		Constructor.m_MaxParticles = 500;
		Constructor.m_startSize = 2.01f;
		Constructor.m_endSize = 0.0f;
		Constructor.m_velocityMin = 0.1f;
		Constructor.m_velocityMax = 1.0f;
		Constructor.m_ShaderProgram = Shader;
		Constructor.m_updateShader = UpdateShader;
		Constructor.m_position = vec3(0);
		
		Emmitter = new GPU_ParticleEmitter();
		Emmitter->initualize(Constructor);
		RainEmmitter = new GPU_ParticleEmitter();

		Constructor.m_lifespanMax = 10.0f;
		Constructor.m_lifespanMin = 10.0f;
		Constructor.m_MaxParticles = 30000;
		Constructor.m_endSize = 0.5f;
		Constructor.m_startSize = 0.5f;
		Constructor.m_velocityMin = 0.0f;
		Constructor.m_velocityMax = 4.0f;
		Constructor.m_updateShader = RainUpdate;
		RainEmmitter->initualize(Constructor);

		//Textures
		RockDiffuse = OGL_Manager->AddTexture("./data/textures/MossyStone2.jpg");
		RockNormal = OGL_Manager->AddTexture("./data/textures/MossyStoneN.jpg");
		//RockDiffuse = OGL_Manager->AddTexture("./data/textures/StoneGrD.jpg");
		//RockNormal = OGL_Manager->AddTexture("./data/textures/StoneGrN.jpg");
		SandDiffuse = OGL_Manager->AddTexture("./data/textures/SandD2.jpg");
		SandNormal = OGL_Manager->AddTexture("./data/textures/SandN2.jpg");

		//WoodDiffuse = OGL_Manager->AddTexture("./data/textures/MossWood.jpg");
		//WoodNormal = OGL_Manager->AddTexture("./data/textures/MossWoodN.jpg");
		WoodDiffuse = OGL_Manager->AddTexture("./data/textures/WoodD.jpg");
		WoodNormal = OGL_Manager->AddTexture("./data/textures/WoodN.jpg");
		WaterDiffuse = OGL_Manager->AddTexture("./data/textures/WaterCD.png");
		WaterNormal = OGL_Manager->AddTexture("./data/textures/WaterCN.jpg");

		SmokeDiffuse = OGL_Manager->AddTexture("./data/textures/Smoke.png");
		RainDiffuse = OGL_Manager->AddTexture("./data/textures/RainDrop.png");

		//Overlay
		Screen = OGL_Manager->AddFullscreenQuadGeometry(0, appBasics->ScreenSize);

		//Render Targets
		int RenderSize = 512;

		FrameBufferXN = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		FrameBufferXP = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		FrameBufferYN = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		FrameBufferYP = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		FrameBufferZN = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		FrameBufferZP = OGL_Manager->GenNewFrameTarget(RenderSize, RenderSize, true);
		
		RenderTargetXN = OGL_Manager->GenNewRenderTarget(FrameBufferXN, GL_RGBA8);
		RenderTargetXP = OGL_Manager->GenNewRenderTarget(FrameBufferXP, GL_RGBA8);
		RenderTargetYN = OGL_Manager->GenNewRenderTarget(FrameBufferYN, GL_RGBA8);
		RenderTargetYP = OGL_Manager->GenNewRenderTarget(FrameBufferYP, GL_RGBA8);
		RenderTargetZN = OGL_Manager->GenNewRenderTarget(FrameBufferZN, GL_RGBA8);
		RenderTargetZP = OGL_Manager->GenNewRenderTarget(FrameBufferZP, GL_RGBA8);

		vector<VertexComplex> Verts;
		vector<unsigned int> Indexs;

		Verts.push_back({ -10000, 0, -10000, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 });
		Verts.push_back({ -10000, 0, 10000, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 5000 });
		Verts.push_back({ 10000, 0, 10000, 1, 0, 1, 0, 0, 1, 0, 0, 0, 5000, 5000 });
		Verts.push_back({ 10000, 0, -10000, 1, 0, 1, 0, 0, 1, 0, 0, 0, 5000, 0 });

		Indexs.push_back(0);
		Indexs.push_back(1);
		Indexs.push_back(2);
		Indexs.push_back(0);
		Indexs.push_back(2);
		Indexs.push_back(3);

		Water = OGL_Manager->AddCustomGeometry(Verts,Indexs);
		SpearModel = OGL_Manager->AddFBXModel("./FBX/soulspear/soulspear.fbx");
		BunnyModel = OGL_Manager->AddFBXModel("./FBX/Bunny.fbx");
		return true;
	}
	else
	{
		return false;
	}
}

void GraphicsAssigment::RenderScene(vec3 CameraPos, vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.25f, appBasics->ScreenSize.x/appBasics->ScreenSize.y, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(CameraPos, LookPos, vec3(0, 1, 0));
	
	OGL_Manager->UseShader(ShaddowsPointTextureBump);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	//OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("LightColour", LightColour);
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("BaseLight", 0.00f);

	

	OGL_Manager->SetRenderTargetAsTexture(RenderTargetXN, 2, "XN");
	OGL_Manager->SetRenderTargetAsTexture(RenderTargetXP, 3, "XP");
	OGL_Manager->SetRenderTargetAsTexture(RenderTargetYN, 4, "YN");
	OGL_Manager->SetRenderTargetAsTexture(RenderTargetYP, 5, "YP");
	OGL_Manager->SetRenderTargetAsTexture(RenderTargetZN, 6, "ZN");
	OGL_Manager->SetRenderTargetAsTexture(RenderTargetZP, 7, "ZP");

	RenderComponants(LightPos, LightRadius, true);

}

void GraphicsAssigment::RenderXP(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(1,0,0), vec3(0, 1, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferXP, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius, false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}
void GraphicsAssigment::RenderXN(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(-1, 0, 0), vec3(0, 1, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferXN, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius, false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}
void GraphicsAssigment::RenderYP(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(0, 1, 0), vec3(1, 0, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferYP, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius, false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}
void GraphicsAssigment::RenderYN(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(0, -1, 0), vec3(-1.0f, 0, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferYN, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius, false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}
void GraphicsAssigment::RenderZP(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(0, 0, 1), vec3(0, 1, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferZP, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius, false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}
void GraphicsAssigment::RenderZN(vec3 LightPos, float LightRadius)
{
	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.5f, 1, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(LightPos, LightPos + vec3(0, 0, -1), vec3(0, 1, 0));
	OGL_Manager->BeginNewDrawTo(FrameBufferZN, vec4(0, 0, 0, 1));
	OGL_Manager->UseShader(PointDepth);
	OGL_Manager->PassInUniform("LightPos", LightPos);
	OGL_Manager->PassInUniform("LightRadius", LightRadius);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	RenderComponants(LightPos, LightRadius,false);
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());
}

void GraphicsAssigment::RenderComponants(vec3 LightPos, float LightRadius, bool Finalpass)
{
	OGL_Manager->PassInUniform("SpecPower", 1.5f);
	OGL_Manager->PassInUniform("LightColour", LightColour);
	OGL_Manager->PassInUniform("Brightness", Brightness);
	OGL_Manager->PassInUniform("SpecIntensity", 1.0f);
	OGL_Manager->SetTexture(RockDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(RockNormal, 1, "normal");

	int BuildingSize = 6;
	int MinX = Map->Map->GetMinX();
	int MaxX = Map->Map->GetMaxX();
	int MinZ = Map->Map->GetMinZ();
	int MaxZ = Map->Map->GetMaxZ();
	int SeedSelector = 2000;

	for (int x = MinX - BuildingSize; x <= MaxX + BuildingSize; ++x)
	{
		for (int y = MinZ - BuildingSize; y <= MaxZ + BuildingSize; ++y)
		{
			if (x%BuildingSize == 0 && y%BuildingSize == 0)
			{
				srand(x + y*SeedSelector);

				int BuildingID = rand() % NumberUniqueBuildings;
				GA_BuildingCluster* ID = Buildings[BuildingID];
				if ((float)rand() / RAND_MAX > 1.5f*Age)
				{
					ID = WholeBuildings[BuildingID];
				}

				float Height = MapTile::GetPerlin(Frequency, Amp, ID->Details.z + x + 0.5f, y + ID->Details.w + 0.5f);
				if (Settlement != 0)
					Height -= Age / 12;

				vec3 BuildingLoc = vec3(x + BuildingSize / 2, Height + BuildingSize / 2, y + BuildingSize / 2);
				vec3 Delta = LightPos - BuildingLoc;
				if (glm::length(Delta) < LightRadius + BuildingSize / 2 && Height >  BuildLevel)
				{
					ID->Draw(vec3(x, Height, y), OGL_Manager);
				}
			}
		}
	}


	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("Brightness", Brightness);
	OGL_Manager->PassInUniform("LightColour", LightColour);
	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTexture(WoodDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(WoodNormal, 1, "normal");

	for (int x = MinX - BuildingSize; x <= MaxX + BuildingSize; ++x)
	{
		for (int y = MinZ - BuildingSize; y <= MaxZ + BuildingSize; ++y)
		{
			if (x%BuildingSize == 0 && y%BuildingSize == 0)
			{
				srand(x + y*SeedSelector);

				int BuildingID = rand() % NumberUniqueBuildings;
				GA_BuildingCluster* ID = Buildings[BuildingID];
				if ((float)rand() / RAND_MAX > 1.5f*Age)
				{
					ID = WholeBuildings[BuildingID];
				}

				float Height = MapTile::GetPerlin(Frequency, Amp, ID->Details.z + x + 0.5f, y + ID->Details.w + 0.5f);

				if (Settlement!=0)
					Height -= (Age / 12);


				vec3 BuildingLoc = vec3(x + BuildingSize / 2, Height + BuildingSize / 2, y + BuildingSize / 2);
				vec3 Delta = LightPos - BuildingLoc;
				if (glm::length(Delta) < LightRadius + BuildingSize / 2 && Height > BuildLevel)
				{
					ID->DrawSpacers(vec3(x, Height, y), OGL_Manager);
				}
			}
		}
	}

	OGL_Manager->SetTexture(SandDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(SandNormal, 1, "normal");
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("LightColour", LightColour);
	OGL_Manager->PassInUniform("Brightness", Brightness);
	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTransform(glm::translate(glm::vec3(0, 0, 0)));
	Map->Draw(OGL_Manager);

	if (ShowModel == 1)
	{
		for (int i = 0; i < NumberModels; ++i)
		{
			vec3 PosDelta = vec3(OribtRadius, -5 * ModelSize, 0);
			PosDelta = glm::rotate(PosDelta, (Time + ((6 * (float)i) / (float)NumberModels) * 3.1415f / 1.5f) / 2, vec3(0, 1, 0));
			OGL_Manager->SetTransform(glm::translate(vec3(0, 0, 0)));
			OGL_Manager->DrawFBX(SpearModel, glm::mat4(vec4(ModelSize, 0, 0, 0), vec4(0, ModelSize, 0, 0), vec4(0, 0, ModelSize, 0), vec4(LightPos + PosDelta, 1)),true);
	
		}
	}
	if (ShowModel == 2)
	{
		for (int i = 0; i < NumberModels; ++i)
		{
			if (Finalpass)
			{
				OGL_Manager->UseShader(ShaddowsGrey);
				OGL_Manager->PassInUniform("LightPos", LightPos);
				OGL_Manager->PassInUniform("LightRadius", LightRadius);
				OGL_Manager->PassInUniform("LightColour", LightColour);
				OGL_Manager->PassInUniform("Brightness", Brightness);
				OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
				OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
				OGL_Manager->PassInUniform("BaseLight", 0.00f);

				OGL_Manager->SetRenderTargetAsTexture(RenderTargetXN, 2, "XN");
				OGL_Manager->SetRenderTargetAsTexture(RenderTargetXP, 3, "XP");
				OGL_Manager->SetRenderTargetAsTexture(RenderTargetYN, 4, "YN");
				OGL_Manager->SetRenderTargetAsTexture(RenderTargetYP, 5, "YP");
				OGL_Manager->SetRenderTargetAsTexture(RenderTargetZN, 6, "ZN");
				OGL_Manager->SetRenderTargetAsTexture(RenderTargetZP, 7, "ZP");
			}

			vec3 PosDelta = vec3(OribtRadius, -2 * ModelSize, 0);
			PosDelta = glm::rotate(PosDelta, (Time + ((6 * (float)i) / (float)NumberModels) * 3.1415f / 1.5f) / 2, vec3(0, 1, 0));
			OGL_Manager->SetTransform(glm::translate(vec3(0,0,0)));
			OGL_Manager->DrawFBX(BunnyModel, glm::mat4(vec4(ModelSize/3, 0, 0, 0), vec4(0, ModelSize/3, 0, 0), vec4(0, 0, ModelSize/3, 0), vec4(LightPos + PosDelta, 1)),false);
	
		}
	}

	//if (ShowModel == 1)
	//{
	//	for (int i = 0; i < NumberModels; ++i)
	//	{
	//		vec3 PosDelta = vec3(OribtRadius, -5 * ModelSize, 0);
	//		PosDelta = glm::rotate(PosDelta, (Time + ((6 * (float)i) / (float)NumberModels) * 3.1415f / 1.5f) / 2, vec3(0, 1, 0));
	//		//OGL_Manager->SetTransform(glm::translate(LightPos + PosDelta));
	//		OGL_Manager->DrawFBX(SpearModel, glm::mat4(vec4(ModelSize, 0, 0, 0), vec4(0, ModelSize, 0, 0), vec4(0, 0, ModelSize, 0), vec4(1,0,1, 1)), true);
	//
	//	}
	//}
	//if (ShowModel == 2)
	//{
	//	for (int i = 0; i < NumberModels; ++i)
	//	{
	//
	//		vec3 PosDelta = vec3(OribtRadius, -5 * ModelSize, 0);
	//		PosDelta = glm::rotate(PosDelta, (Time + ((6 * (float)i) / (float)NumberModels) * 3.1415f / 1.5f) / 2, vec3(0, 1, 0));
	//		//OGL_Manager->SetTransform(glm::translate(LightPos+PosDelta));
	//		OGL_Manager->DrawFBX(BunnyModel, glm::mat4(vec4(ModelSize / 3, 0, 0, 0), vec4(0, ModelSize / 3, 0, 0), vec4(0, 0, ModelSize / 3, 0), vec4(1,0,1, 1)), false);
	//
	//	}
	//}

}