#pragma once
#include "GL_Manager.h"
struct GPU_PE_Constructer
{
	unsigned int m_MaxParticles;
	float m_lifespanMin;
	float m_lifespanMax;
	float m_velocityMin;
	float m_velocityMax;
	float m_startSize;
	float m_endSize;

	glm::vec3 m_position;
	glm::vec4 m_startColour;
	glm::vec4 m_endColour;

	unsigned int m_ShaderProgram;
	unsigned int m_updateShader;
};

struct GPUParticle
{
	GPUParticle() :lifetime(1), lifespan(0){}
	vec3 position;
	vec3 velocity;
	float lifetime;
	float lifespan;

};

class GPU_ParticleEmitter
{
public:
	GPU_ParticleEmitter();
	virtual ~GPU_ParticleEmitter();

	void initualize(GPU_PE_Constructer ConstructionInfo);
	void Draw(float Time,GL_Manager* ManagerRef,const glm::mat4& a_cameraTransform,const glm::mat4& a_projectionView);
	void DrawAt(glm::vec3 Loc,float Time, GL_Manager* ManagerRef, const glm::mat4& a_cameraTransform, const glm::mat4& a_projectionView,int Texture);
	void DrawRainAt(glm::vec3 LocUpper, vec3 LocLower, float Time, GL_Manager* ManagerRef, const glm::mat4& a_cameraTransform, const glm::mat4& a_projectionView, int Texture);

protected:
	void CreateBuffers(); //Creates 2 Buffers
	void CreateUpdateShader();
	void CreateDrawShader();


	GPUParticle* m_particles;
	unsigned int m_activeBuffer;
	unsigned int m_vao[2];
	unsigned int m_vbo[2];

	float m_LastDrawTime;
	vec3 LastPos;

	GPU_PE_Constructer EmitterDetails;




};

