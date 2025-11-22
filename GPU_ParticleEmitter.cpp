#include "GPU_ParticleEmitter.h"
#include "GL_Manager.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"

GPU_ParticleEmitter::GPU_ParticleEmitter() :m_particles(nullptr)
{
	m_LastDrawTime = 0;
	m_vao[0] = 0;
	m_vao[1] = 0;
	m_vbo[0] = 0;
	m_vbo[1] = 0;
}


GPU_ParticleEmitter::~GPU_ParticleEmitter()
{
	delete[] m_particles;
	glDeleteVertexArrays(2, m_vao);
	glDeleteBuffers(2, m_vbo);
}

void GPU_ParticleEmitter::initualize(GPU_PE_Constructer ConstructionInfo)
{
	EmitterDetails = ConstructionInfo;
	m_particles = new GPUParticle[EmitterDetails.m_MaxParticles];
	m_activeBuffer = 0;
	LastPos = EmitterDetails.m_position;

	CreateBuffers();
	CreateUpdateShader();
	CreateDrawShader();
}

void GPU_ParticleEmitter::Draw(float Time, GL_Manager* ManagerRef,const glm::mat4& a_cameraTransform, const glm::mat4& a_projectionView)
{
	ManagerRef->UseUpdateShader(EmitterDetails.m_updateShader);
	ManagerRef->PassInUniform("lifeMin", EmitterDetails.m_lifespanMin);
	ManagerRef->PassInUniform("lifeMax", EmitterDetails.m_lifespanMax);
	ManagerRef->PassInUniform("velocityMax", EmitterDetails.m_velocityMax);
	ManagerRef->PassInUniform("velocityMin", EmitterDetails.m_velocityMin);
	ManagerRef->PassInUniform("time", Time);
	float DeltaTime = Time - m_LastDrawTime;
	m_LastDrawTime = Time;
	if (DeltaTime >  0.5f)
	{
		DeltaTime = 0.5f;
	}
	if (DeltaTime < 0)
		DeltaTime = 0;
	ManagerRef->PassInUniform("deltaTime", DeltaTime);
	ManagerRef->PassInUniform("emitterPosition", EmitterDetails.m_position);
	ManagerRef->PassInUniform("lastPosition", LastPos);//differs in notation from tutorial
	glEnable(GL_RASTERIZER_DISCARD);

	glBindVertexArray(m_vao[m_activeBuffer]);
	unsigned int otherBuffer = (m_activeBuffer + 1) % 2;
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_vbo[otherBuffer]);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);
	glEndTransformFeedback();
	glDisable(GL_RASTERIZER_DISCARD);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);


	ManagerRef->UseShader(EmitterDetails.m_ShaderProgram);
	ManagerRef->PassInUniform("sizeStart", EmitterDetails.m_startSize);
	ManagerRef->PassInUniform("sizeEnd", EmitterDetails.m_endSize);
	ManagerRef->PassInUniform("colourStart", EmitterDetails.m_startColour);
	ManagerRef->PassInUniform("colourEnd", EmitterDetails.m_endColour);
	ManagerRef->PassInUniform("projectionView", a_projectionView);
	ManagerRef->PassInUniform("cameraTransform", a_cameraTransform);

	glBindVertexArray(m_vao[otherBuffer]);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);

	m_activeBuffer = otherBuffer;



}



void GPU_ParticleEmitter::DrawRainAt(glm::vec3 LocUpper, vec3 LocLower, float Time, GL_Manager* ManagerRef, const glm::mat4& a_cameraTransform, const glm::mat4& a_projectionView, int Texture)
{

	ManagerRef->UseUpdateShader(EmitterDetails.m_updateShader);
	ManagerRef->PassInUniform("lifeMin", EmitterDetails.m_lifespanMin);
	ManagerRef->PassInUniform("lifeMax", EmitterDetails.m_lifespanMax);
	ManagerRef->PassInUniform("velocityMax", EmitterDetails.m_velocityMax);
	ManagerRef->PassInUniform("velocityMin", EmitterDetails.m_velocityMin);
	ManagerRef->PassInUniform("time", Time);
	float DeltaTime = Time - m_LastDrawTime;
	m_LastDrawTime = Time;
	if (DeltaTime >  0.5f)
	{
		DeltaTime = 0.5f;
	}
	if (DeltaTime < 0)
		DeltaTime = 0;
	ManagerRef->PassInUniform("deltaTime", DeltaTime);

	ManagerRef->PassInUniform("emitterPosition", LocUpper); //differs in notation from tutorial
	ManagerRef->PassInUniform("lastPosition", LocLower);
	glEnable(GL_RASTERIZER_DISCARD);

	glBindVertexArray(m_vao[m_activeBuffer]);
	unsigned int otherBuffer = (m_activeBuffer + 1) % 2;
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_vbo[otherBuffer]);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);
	glEndTransformFeedback();
	glDisable(GL_RASTERIZER_DISCARD);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);


	ManagerRef->UseShader(EmitterDetails.m_ShaderProgram);
	ManagerRef->PassInUniform("sizeStart", EmitterDetails.m_startSize);
	ManagerRef->PassInUniform("sizeEnd", EmitterDetails.m_endSize);
	ManagerRef->PassInUniform("colourStart", EmitterDetails.m_startColour);
	ManagerRef->PassInUniform("colourEnd", EmitterDetails.m_endColour);
	ManagerRef->PassInUniform("projectionView", a_projectionView);
	ManagerRef->PassInUniform("cameraTransform", a_cameraTransform);
	ManagerRef->SetTexture(Texture, 0, "diffuse");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vao[otherBuffer]);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);
	glDisable(GL_BLEND);
	m_activeBuffer = otherBuffer;
}

void GPU_ParticleEmitter::DrawAt(glm::vec3 Loc, float Time, GL_Manager* ManagerRef, const glm::mat4& a_cameraTransform, const glm::mat4& a_projectionView,int Texture)
{

	ManagerRef->UseUpdateShader(EmitterDetails.m_updateShader);
	ManagerRef->PassInUniform("lifeMin", EmitterDetails.m_lifespanMin);
	ManagerRef->PassInUniform("lifeMax", EmitterDetails.m_lifespanMax);
	ManagerRef->PassInUniform("velocityMax", EmitterDetails.m_velocityMax);
	ManagerRef->PassInUniform("velocityMin", EmitterDetails.m_velocityMin);
	ManagerRef->PassInUniform("time", Time);
	float DeltaTime = Time - m_LastDrawTime;
	m_LastDrawTime = Time;
	if (DeltaTime >  0.5f)
	{
		DeltaTime = 0.5f;
	}
	if (DeltaTime < 0)
		DeltaTime = 0;
	ManagerRef->PassInUniform("deltaTime", DeltaTime);
	ManagerRef->PassInUniform("emitterPosition", Loc); //differs in notation from tutorial
	ManagerRef->PassInUniform("lastPosition", LastPos);
	glEnable(GL_RASTERIZER_DISCARD);

	glBindVertexArray(m_vao[m_activeBuffer]);
	unsigned int otherBuffer = (m_activeBuffer + 1) % 2;
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_vbo[otherBuffer]);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);
	glEndTransformFeedback();
	glDisable(GL_RASTERIZER_DISCARD);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);


	ManagerRef->UseShader(EmitterDetails.m_ShaderProgram);
	ManagerRef->PassInUniform("sizeStart", EmitterDetails.m_startSize);
	ManagerRef->PassInUniform("sizeEnd", EmitterDetails.m_endSize);
	ManagerRef->PassInUniform("colourStart", EmitterDetails.m_startColour);
	ManagerRef->PassInUniform("colourEnd", EmitterDetails.m_endColour);
	ManagerRef->PassInUniform("projectionView", a_projectionView);
	ManagerRef->PassInUniform("cameraTransform", a_cameraTransform);
	ManagerRef->SetTexture(Texture, 0, "diffuse");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vao[otherBuffer]);
	glDrawArrays(GL_POINTS, 0, EmitterDetails.m_MaxParticles);
	glDisable(GL_BLEND);
	m_activeBuffer = otherBuffer;

	LastPos = Loc;
}

void GPU_ParticleEmitter::CreateBuffers() //Creates 2 Buffers
{
	glGenVertexArrays(2, m_vao);
	glGenBuffers(2, m_vbo);

	glBindVertexArray(m_vao[0]);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, EmitterDetails.m_MaxParticles*sizeof(GPUParticle), m_particles, GL_STREAM_DRAW);

	glEnableVertexAttribArray(0); //position;
	glEnableVertexAttribArray(1); //velocity;
	glEnableVertexAttribArray(2); //lifetime;
	glEnableVertexAttribArray(3); //lifespan;

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 12);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 24);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 28);


	glBindVertexArray(m_vao[1]);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, EmitterDetails.m_MaxParticles*sizeof(GPUParticle), 0, GL_STREAM_DRAW);

	glEnableVertexAttribArray(0); //position;
	glEnableVertexAttribArray(1); //velocity;
	glEnableVertexAttribArray(2); //lifetime;
	glEnableVertexAttribArray(3); //lifespan;

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 12);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 24);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GPUParticle), ((char*)0) + 28);


	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

}
void GPU_ParticleEmitter::CreateUpdateShader()
{
	
}
void GPU_ParticleEmitter::CreateDrawShader()
{

}