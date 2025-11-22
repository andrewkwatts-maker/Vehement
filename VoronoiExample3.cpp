#include "VoronoiExample3.h"
#include "Camera.h"
#include "Clock.h"
#include "GL_Manager.h"
#include "glm\glm.hpp"
#include "glm\ext.hpp"

#include "Inputs.h"
VoronoiExample3::VoronoiExample3()
{
}


VoronoiExample3::~VoronoiExample3()
{
}


bool VoronoiExample3::update()
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
void VoronoiExample3::draw()
{
	vec3 LightPosition = appBasics->AppCamera->GetPos() + appBasics->AppCamera->GetDirVector()*1.0f;
	OGL_Manager->UseShader(PointTexturedBump);
	OGL_Manager->PassInUniform("LightPos", LightPosition);
	OGL_Manager->PassInUniform("LightColour", vec3(1, 1.0f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.5f);
	OGL_Manager->PassInUniform("Brightness", 3.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());

	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTexture(RockDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(RockNormal, 1, "normal");
	OGL_Manager->SetTransform(glm::translate(glm::vec3(0,0,0)));

	Box->Draw();
	//Box->DrawEdges(vec4(1));
	
	VoronoiCell* CellID = nullptr;
	float Clossest = FLT_MAX;
	int Ref = -1;
	for (int i = 0; i < Box->Cells.size();++i)
	{
		
		if (glm::length(Box->Cells[i]->Location - LightPosition) < Clossest)
		{
			Clossest = glm::length(Box->Cells[i]->Location - LightPosition);
			CellID = Box->Cells[i];
			Ref = i;
		}
	}
	
	CellID->DrawEdges(vec4(1));
	
	if (appBasics->AppInputs->IsKeyDown(GLFW_KEY_X))
	{
	
		Box = new VoronoiBoxContainer(vec3(0), vec3(2, 7, 2));
		for (int i = 0; i < 100; ++i)
		{
			if (i % 2 == 1)
				Box->AddRandomSeed(VoroType::eSOLID);
			else
				Box->AddRandomSeed(VoroType::eVOID);
		}
		Box->CalculateAllSeeds();
	}

	//Cell->Draw();
	//
	//Cell->DrawEdges(glm::vec4(1, 1, 1, 0.2f));

	Application::draw();
}
bool VoronoiExample3::startup()
{
	if (Application::startup())
	{
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		RockDiffuse = OGL_Manager->AddTexture("./data/textures/Stone.jpg");
		RockNormal = OGL_Manager->AddTexture("./data/textures/StoneN.jpg");

		//Cell = new VoronoiCell(vec3(0),1);
		//for (int i = 0; i < 10; ++i)
		//{
		//	vec3 Loc = vec3(rand() % 2000-1000, rand() % 2000-1000, rand() % 2000-1000);
		//	Loc /= 100;
		//	Cell->GenFaceFromSeed(VoronoiSeed(Loc, 1.0f));
		//}
		//Cell->CopyFaceEdges();
		//Cell->Type = VoroType::eSOLID;
		//Cell->Gen_GL_Buffers();

		Box = new VoronoiBoxContainer(vec3(0), vec3(2, 7,2));
		for (int i = 0; i < 47; ++i)
		{
			if (i%2 == 1)
				Box->AddRandomSeed(VoroType::eSOLID);
			else
				Box->AddRandomSeed(VoroType::eVOID);
		}
		Box->CalculateAllSeeds();
		return true;
	}
	else
	{
		return false;
	}

}