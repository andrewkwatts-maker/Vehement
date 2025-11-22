#include "GraphicsTut10.h"
#include "GL_Manager.h"
#include "Camera.h"
#include "Clock.h"
GraphicsTut10::GraphicsTut10()
{
}


GraphicsTut10::~GraphicsTut10()
{
}


bool GraphicsTut10::update()
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
void GraphicsTut10::draw()
{
	Emitter->Draw(appBasics->AppClock->GetProgramTime().Second,OGL_Manager,appBasics->AppCamera->GetWorldTransform(),appBasics->AppCamera->GetProjectionView());

	//Application::draw();
}


bool GraphicsTut10::startup()
{
	if (Application::startup())
	{

		const char* Varyiances[] = { "position", "velocity", "lifetime", "lifespan" };
		int UpdateShader = OGL_Manager->AddUpdateShader("./Shaders/VS_gpuParticleUpdate.vert", Varyiances, 4);
		int Shader = OGL_Manager->AddShaders("./Shaders/VS_gpuParticle.vert", "./Shaders/FS_gpuParticle.frag", "./Shaders/GS_gpuParticle.geom");

		GPU_PE_Constructer Constructor;
		Constructor.m_endColour = glm::vec4(1, 1, 0, 1);
		Constructor.m_startColour = glm::vec4(1, 0, 0, 1);
		Constructor.m_lifespanMin = 1.0f;
		Constructor.m_lifespanMax = 4.0f;
		Constructor.m_MaxParticles = 50000;
		Constructor.m_startSize = 0.4f;
		Constructor.m_endSize = 0.0f;
		Constructor.m_velocityMin = 0.5f;
		Constructor.m_velocityMax = 1.0f;
		Constructor.m_ShaderProgram = Shader;
		Constructor.m_updateShader = UpdateShader;
		Constructor.m_position = vec3(0);

		Emitter = new GPU_ParticleEmitter();
		Emitter->initualize(Constructor);

		return true;
	}
	else
	{
		return false;
	}
}
