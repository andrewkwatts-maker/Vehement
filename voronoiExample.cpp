#include "voronoiExample.h"
#include "VORO_MATHS.h"
#include "Clock.h"
#include "Inputs.h"
#include "Camera.h"
#include "GL_Manager.h"
#include "glm\ext.hpp"
#include "Expandable3D_deque.h"
voronoiExample::voronoiExample()
{
}


voronoiExample::~voronoiExample()
{
}


double rnd()
{
	return double(rand()) / RAND_MAX;
}

bool voronoiExample::update()
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


void voronoiExample::draw()
{
	vec3 LightPosition = appBasics->AppCamera->GetDirVector()*5.0f + appBasics->AppCamera->GetPos();
	vec3 InspectPosition = appBasics->AppCamera->GetDirVector()*10.0f + appBasics->AppCamera->GetPos();
	//Gizmos::addTransform(mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(LightPosition, 1)));
	Gizmos::addTransform(mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(InspectPosition, 1)));

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
	OGL_Manager->SetTransform(glm::translate(glm::vec3(0, 0, 0)));

	//CELL->Draw(vec3(1,0,0),appBasics->AppCamera->GetPos());
	//STD_CELL->Draw();
	//STD_CELL->DrawEdges(vec3(1, 0, 0), LightPosition);

	for (int Seed = 0; Seed < SEEDS.size(); ++Seed)
	{
		bool Acceptable = true;
		int Count = 0;
		for (int Face = 0; Face < CELLS[Seed]->Faces.size(); ++Face)
		{
			if (!CELLS[Seed]->Faces[Face]->Face->IsPointUnder(InspectPosition))
			{
					Acceptable = false;
			}
		}
	
		if (Acceptable == true)
		{
			if (appBasics->AppInputs->IsKeyDown(GLFW_KEY_1))
			{
				Visible[Seed] = false;
			}
			if (appBasics->AppInputs->IsKeyDown(GLFW_KEY_2))
			{
	
				Visible[Seed] = true;
			}
			//CELLS[Seed]->DrawEdges(vec3(1, 0, 0), LightPosition);
		}
		else
		{
			//CELLS[Seed]->Draw();
			//CELLS[Seed]->DrawEdges(vec3(0, 1, 0), LightPosition);
		}
	
		if (Visible[Seed])
			CELLS[Seed]->Draw();
		else
		{
			CELLS[Seed]->DrawEdges(vec3(1, 0, 0));
		}
	
	}


	//for (int DivCellID = 0; DivCellID < Cont->Cells.size(); ++DivCellID)
	//{
	//	for (int CellID = 0; CellID < Cont->Cells[DivCellID].size(); ++CellID)
	//	{
	//		Cont->Cells[DivCellID][CellID]->Draw();
	//		//Cont->Cells[DivCellID][CellID]->DrawEdges(vec3(1),LightPosition);
	//	}
	//}

	Application::draw();
}
bool voronoiExample::startup()
{
	if (Application::startup())
	{
		//Shader
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		RockDiffuse = OGL_Manager->AddTexture("./data/textures/Stone.jpg");
		RockNormal = OGL_Manager->AddTexture("./data/textures/StoneN.jpg");
		float Time;


		srand(4);

		S = 7.0f;
		Seeds = 100;
		Cont = new VORO_CONTAINER(vec3(0), vec3(S), vec3(12, 3, 12));

		//Base = vec3(S/2);
		//Cont = new VORO_CONTAINER(vec3(-20,-5,-20), vec3(20,5,20), vec3(12,3,12));
		//Time = appBasics->AppClock->RunClock();
		//Cont->FillWithCubes();
		//Time = appBasics->AppClock->RunClock();
		//printf("%f", 1 / Time);


		//STD_CELL = (VORO_CELL*)Cont->GenNewFromPoint(&VORO_SEED(vec3(2.004f,2.0034f,3.445f), 1));
		//
		//
		////float Time = appBasics->AppClock->RunClock();
		//for (int Seed = 0; Seed < 22; ++Seed)
		//{
		//	//CELL->AddSeed(&VORO_SEED(vec3(rnd()*S, rnd()*S, rnd()*S), 1));
		//	STD_CELL->AddSeed(&VORO_SEED(vec3(rnd()*S, rnd()*S, rnd()*S), 1));
		//}
		//STD_CELL->Gen_GL_Buffers();
		//
		//
		//
		Time = appBasics->AppClock->RunClock();
		for (int Seed = 0; Seed < Seeds; ++Seed)
		{
			SEEDS.push_back(nullptr);
			CELLS.push_back(nullptr);
			Visible.push_back(false);
		}
		
		
		for (int Seed = 0; Seed < Seeds; ++Seed)
		{
			SEEDS[Seed] = new VORO_SEED(vec3(rnd()*S, rnd()*S, rnd()*S), 1);
		}
		
		for (int Seed1 = 0; Seed1 < Seeds; ++Seed1)
		{
			CELLS[Seed1] = (VORO_CELL*)Cont->GenNewFromPoint(SEEDS[Seed1]);
			for (int Seed2 = 0; Seed2 < Seeds; ++Seed2)
			{
				if (Seed1 != Seed2)
					CELLS[Seed1]->AddSeed(SEEDS[Seed2]);
			}
			CELLS[Seed1]->Gen_GL_Buffers();
		}
		Time = appBasics->AppClock->RunClock();
		printf("%f", 1 / Time);


		//CELLS[7]->Gen_GL_Buffers();
		//CELL = Cont->GenNewFromPoint(&VORO_SEED(vec3(S/2), 1));

		//Time = appBasics->AppClock->RunClock();
		//printf("%f", 1/Time);
		

		return true;
	}
	else
	{
		return false;
	}

}
