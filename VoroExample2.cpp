#include "VoroExample2.h"
#include "Camera.h"
#include "Inputs.h"
#include "GL_Manager.h"
#include "glm\ext.hpp"
VoroExample2::VoroExample2()
{
}


VoroExample2::~VoroExample2()
{
}


bool VoroExample2::update()
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


void VoroExample2::draw()
{
	//Draw
	vec3 LightPosition = appBasics->AppCamera->GetDirVector()*5.0f + appBasics->AppCamera->GetPos();
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



	//Build
	vec3 InspectPosition = appBasics->AppCamera->GetDirVector()*10.0f + appBasics->AppCamera->GetPos();
	VoroType typeInsert = VoroType::eEmpty;
	if (appBasics->AppInputs->KeyPressed(GLFW_KEY_C))
		typeInsert = VoroType::eSOLID;
	if (appBasics->AppInputs->KeyPressed(GLFW_KEY_V))
		typeInsert = VoroType::eVOID;

	Space->Build(InspectPosition, typeInsert);


	vHitE1 = 0;
	vHitE2 = 0;
	vHitE3 = 0;
	vHitE4 = 0;
	vHitE5 = 0;
	vHitE6 = 0;
	vHitE7 = 0;
	vHitE8 = 0;
	vHitE9 = 0;
	vHitE10 = 0;
	vHitE11 = 0;
	vHitE12 = 0;
	vHitE13 = 0;
	vHitE14 = 0;
	vHitE15 = 0;
	vHitE16 = 0;


	VORO_CELL* CellTemp = Space->DrawInspectionEffect(InspectPosition);
	CellTemp->DrawEdges(vec3(1, 1, 0));
	delete CellTemp;

	printf("1:%i,%i,%i,%i, 5:%i,%i,%i,%i, 9:%i,%i,%i,%i, 13:%i,%i,%i,%i \n", vHitE1, vHitE2, vHitE3, vHitE4, vHitE5, vHitE6, vHitE7, vHitE8, vHitE9, vHitE10, vHitE11, vHitE12, vHitE13, vHitE14, vHitE15, vHitE16);
	//printf("F..1:%i 2:%i 3:%i 4:%i 5:%i 6:%i 7:%i 8:%i \n", vHitF1, vHitF2, vHitF3, vHitF4, vHitF5, vHitF6, vHitF7, vHitF8);
	

	Space->Draw();

	Application::draw();
}
bool VoroExample2::startup()
{
	if (Application::startup())
	{
		Space = new VORO_Space(1);
		//Shader
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		RockDiffuse = OGL_Manager->AddTexture("./data/textures/Stone.jpg");
		RockNormal = OGL_Manager->AddTexture("./data/textures/StoneN.jpg");
		return true;
	}
	else
	{
		return false;
	}
}