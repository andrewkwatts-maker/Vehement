#include "GraphicsTut8.h"


#include "GL_Manager.h"
#include "Camera.h"
#include "Clock.h"
GraphicsTut8::GraphicsTut8()
{
}


GraphicsTut8::~GraphicsTut8()
{
}

bool GraphicsTut8::update()
{
	if (Application::update())
	{
		ParticleSystem->Update(appBasics->AppClock->GetDelta(), appBasics->AppCamera->GetWorldTransform());
		return true;
	}
	else
		return false;
}
void GraphicsTut8::draw()
{
	OGL_Manager->UseShader(ParticleShaderProgram);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	ParticleSystem->Draw();
	Application::draw();
}
bool GraphicsTut8::startup()
{
	if (Application::startup())
	{
		ParticleShaderProgram = OGL_Manager->AddShaders("./Shaders/VS_BasicParticle.vert", "./Shaders/FS_BasicParticle.frag");
		ParticleSystem = new ParticleEmitter();
		ParticleSystem->initalise(1000, 100, 0.1f, 1.0f, 1, 5, 1, 0.1f, vec4(1, 0, 0, 1), vec4(1, 1, 0, 1));
		return true;
	}
	else
		return false;
}