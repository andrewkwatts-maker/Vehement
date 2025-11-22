#include "GraphicsTut7.h"
#include "GL_Manager.h"
#include "Camera.h"
#include "Clock.h"
GraphicsTut7::GraphicsTut7()
{
}


GraphicsTut7::~GraphicsTut7()
{
}



bool GraphicsTut7::update()
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
void GraphicsTut7::draw()
{
	Time += appBasics->AppClock->GetDelta();
	float scale = 0.001f;
	OGL_Manager->UseShader(Point_Textured_Bump);
	OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("LightColour", vec3(1, 0.5f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("SpecIntensity", 1.0f);
	OGL_Manager->PassInUniform("Brightness", 14.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	//OGL_Manager->DrawFBX(PyroModel, glm::mat4(vec4(scale, 0, 0, 0), vec4(0, scale, 0, 0), vec4(0, 0, scale, 0), vec4(0, 0, 0, 1)),Time);

	for (int Model = 0; Model < 10; ++Model)
	{
		OGL_Manager->DrawFBX(PyroModel, glm::mat4(vec4(scale, 0, 0, 0), vec4(0, scale, 0, 0), vec4(0, 0, scale, 0), vec4(Model*2-10, 0, 0, 1)), Time+Model*0.5f);
	}
	Application::draw();
}


bool GraphicsTut7::startup()
{
	if (Application::startup())
	{
		PyroModel = OGL_Manager->AddFBXModel("./FBX/Pyro/pyro.fbx");
		Point_Textured_Bump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump_Spec_Anim.vert", "./Shaders/FS_PointLight_Textured_Bump_Spec_Anim.frag");
		Time = 0;
		return true;
	}
	else
	{
		return false;
	}
}