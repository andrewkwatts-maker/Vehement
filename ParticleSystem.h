#pragma once
#include "aie\Gizmos.h"
#include "glm\vec3.hpp"
#include "glm\vec4.hpp"
#include "glm\mat4x4.hpp"
using glm::vec3;
using glm::vec4;

struct Particle{
	vec3 Position;
	vec3 Velocity;
	vec4 Colour;

	float Size;
	float LifeTime;
	float LifeSpan;
	float Rotation;
};

struct ParticleVertex
{
	vec4 Position;
	vec4 Colour;
};

class ParticleEmitter
{
public:
	void initalise(unsigned int a_maxParticles, unsigned int a_emitRate, float a_lifetimeMin, float a_lifetimeMax, float a_velocityMin, float a_velocityMax, float a_startSize, float a_endSize, const vec4& a_startColour, const vec4& a_endColour);
	void Emit();
	void Update(float Delta,const glm::mat4& a_CameraTransform);
	void Draw();
	ParticleEmitter();
	virtual ~ParticleEmitter();
protected:
	Particle* m_particles;
	unsigned int m_firstDead;
	unsigned int m_maxParticles;

	unsigned int m_vao, m_vbo, m_ibo;
	ParticleVertex* m_vertexData;


	vec3 m_position;

	float m_emitTimer;
	float m_emitRate;

	float m_lifespanMin;
	float m_lifespanMax;

	float m_velocityMin;
	float m_velocityMax;

	float m_startSize;
	float m_endSize;

	vec4 m_startColour;
	vec4 m_endColour;


};