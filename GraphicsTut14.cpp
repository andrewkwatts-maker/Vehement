#include "GraphicsTut14.h"
#include "GL_Manager.h"
#include "Camera.h"


GraphicsTut14::GraphicsTut14()
{
}


GraphicsTut14::~GraphicsTut14()
{
}


bool GraphicsTut14::update()
{
	if (Application::update())
	{
		return true;
	}
	else
	{
		return false;
	}

}
void GraphicsTut14::draw()
{
	OGL_Manager->UseShader(PointLight);
	OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos()+15.0f*appBasics->AppCamera->GetDirVector());
	OGL_Manager->PassInUniform("LightColour", vec3(1.0f, 1.0f, 1.0f));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("Brightness", 14.0f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->DrawFBX(BunnyModel, vec3(0, 0, 0));


	//OGL_Manager->UseShader(Point_Textured);
	//OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos()+15.0f*appBasics->AppCamera->GetDirVector());
	//OGL_Manager->PassInUniform("LightColour", vec3(1, 0.5f, 1));
	//OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	//OGL_Manager->PassInUniform("SpecPower", 1.0f);
	//OGL_Manager->PassInUniform("Brightness", 4.5f);
	//OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	////OGL_Manager->SetTexture(SpearTex, 0, "diffuse");
	//OGL_Manager->DrawFBX(SpearModel, vec3(-5, 0, -5));
	//OGL_Manager->DrawFBX(SpearModel, vec3(5, 0, -5));
	//OGL_Manager->DrawFBX(SpearModel, vec3(5, 0, 5));
	//OGL_Manager->DrawFBX(SpearModel, vec3(-5, 0, 5));

	OGL_Manager->UseShader(Point_Textured_bump);
	OGL_Manager->SetTexture(GroundTex, 0, "diffuse");
	OGL_Manager->SetTexture(GroundN, 1, "normal");
	OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos() + 15.0f*appBasics->AppCamera->GetDirVector());
	OGL_Manager->PassInUniform("LightColour", vec3(1, 1.0f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("SpecIntensity", 0.7f);
	OGL_Manager->PassInUniform("Brightness", 14.0f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->DrawCustomGeometry(Ground,vec3(0));
}


bool GraphicsTut14::startup()
{
	if (Application::startup())
	{
		//Models
		BunnyModel = OGL_Manager->AddFBXModel("./FBX/Bunny.fbx");
		SpearModel = OGL_Manager->AddFBXModel("./FBX/soulspear/soulspear.fbx");

		//Shaders
		PointLight = OGL_Manager->AddShaders("./Shaders/VS_PointLight.vert", "./Shaders/FS_PointLight.frag");
		DirectionalLight = OGL_Manager->AddShaders("./Shaders/VS_DirectionalLight.vert", "./Shaders/FS_DirectionalLight.frag");
		Textured = OGL_Manager->AddShaders("./Shaders/VS_Textured.vert", "./Shaders/FS_Textured.frag");
		Point_Textured = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured.vert", "./Shaders/FS_PointLight_Textured.frag");
		Point_Textured_bump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		GroundTex = OGL_Manager->AddTexture("./data/textures/Tiles.png");
		GroundN = OGL_Manager->AddTexture("./data/textures/TilesN.jpg");
		vector<VertexComplex> Verts;
		vector<unsigned int> Indexs;

		Verts.push_back({ -15, 0, 15, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 4 });
		Verts.push_back({ 15, 0, 15, 1, 0, 1, 0, 0, 1, 0, 0, 0, 4, 4 });
		Verts.push_back({ 15, 0, -15, 1, 0, 1, 0, 0, 1, 0, 0, 0, 4, 0 });
		Verts.push_back({ -15, 0, -15, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 });

		Indexs.push_back(0);
		Indexs.push_back(1);
		Indexs.push_back(2);
		Indexs.push_back(0);
		Indexs.push_back(2);
		Indexs.push_back(3);

		Ground = OGL_Manager->AddCustomGeometry(Verts, Indexs);

		return true;
	}
	else
	{
		return false;
	}
}