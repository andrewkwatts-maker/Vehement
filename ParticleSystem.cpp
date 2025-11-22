#include "ParticleSystem.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\ext.hpp"

ParticleEmitter::ParticleEmitter()
	: m_particles(nullptr),
		m_firstDead(0),
		m_maxParticles(0),
		m_position(0,0,0),
		m_vao(0), m_vbo(0), m_ibo(0),
		m_vertexData(nullptr){



}

ParticleEmitter::~ParticleEmitter()
{
	delete[] m_particles;
	delete[] m_vertexData;

	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteBuffers(1, &m_ibo);


}

void ParticleEmitter::initalise(unsigned int a_maxParticles, unsigned int a_emitRate, float a_lifetimeMin, float a_lifetimeMax, float a_velocityMin, float a_velocityMax, float a_startSize, float a_endSize, const vec4& a_startColour, const vec4& a_endColour)
{
	m_emitTimer = 0;
	m_emitRate = 1.0f / a_emitRate;

	m_startColour = a_startColour;
	m_endColour = a_endColour;

	m_startSize = a_startSize;
	m_endSize = a_endSize;

	m_velocityMin = a_velocityMin;
	m_velocityMax = a_velocityMax;

	m_lifespanMin = a_lifetimeMin;
	m_lifespanMax = a_lifetimeMax;

	m_maxParticles = a_maxParticles;

	m_particles = new Particle[m_maxParticles];
	m_firstDead = 0;

	m_vertexData = new ParticleVertex[m_maxParticles * 4];



	unsigned int* indexData = new unsigned int[m_maxParticles * 6];

	for (unsigned int i = 0; i < m_maxParticles; ++i)
	{
		indexData[i * 6 + 0] = i * 4 + 0;
		indexData[i * 6 + 1] = i * 4 + 1;
		indexData[i * 6 + 2] = i * 4 + 2;

		indexData[i * 6 + 3] = i * 4 + 0;
		indexData[i * 6 + 4] = i * 4 + 2;
		indexData[i * 6 + 5] = i * 4 + 3;
	}

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ibo);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, m_maxParticles * 4 * sizeof(ParticleVertex), m_vertexData, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_maxParticles * 6 * sizeof(unsigned int), indexData, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0); //position
	glEnableVertexAttribArray(1); //colour

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), ((char*)0)+16);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	delete[] indexData;


}

void ParticleEmitter::Emit()
{
	if (m_firstDead >= m_maxParticles)
		return;

	Particle& particle = m_particles[m_firstDead++];

	particle.Position = m_position;

	particle.LifeTime = 0;
	particle.LifeSpan = (rand() / (float)RAND_MAX)*(m_lifespanMax - m_lifespanMin) + m_lifespanMin;

	particle.Colour = m_startColour;
	particle.Size = m_startSize;

	float velocity = (rand() / (float)RAND_MAX)*(m_velocityMax - m_velocityMin) + m_velocityMin;

	particle.Velocity.x = (rand() / (float)RAND_MAX) * 2 - 1;
	particle.Velocity.y = (rand() / (float)RAND_MAX) * 2 - 1;
	particle.Velocity.z = (rand() / (float)RAND_MAX) * 2 - 1;
	particle.Velocity = glm::normalize(particle.Velocity)*velocity;

}

void ParticleEmitter::Update(float Delta, const glm::mat4& a_CameraTransform)
{
	m_emitTimer += Delta;

	while (m_emitTimer > m_emitRate)
	{
		Emit();
		m_emitTimer -= m_emitRate;
	}

	unsigned int quad = 0;

	for (unsigned int i = 0; i < m_firstDead; ++i)
	{
		Particle* particle = &m_particles[i];

		particle->LifeTime += Delta;
		if (particle->LifeTime>particle->LifeSpan)
		{
			*particle = m_particles[m_firstDead - 1];
			m_firstDead--;
		}
		else
		{
			particle->Position += particle->Velocity*Delta;
			particle->Size = glm::mix(m_startSize, m_endSize, particle->LifeTime / particle->LifeSpan);
			particle->Colour = glm::mix(m_startColour, m_endColour, particle->LifeTime / particle->LifeSpan);

			float HalfSize = particle->Size / 2.0f;

			m_vertexData[quad * 4 + 0].Position = vec4(HalfSize, HalfSize, 0, 1);
			m_vertexData[quad * 4 + 0].Colour = particle->Colour;

			m_vertexData[quad * 4 + 1].Position = vec4(-HalfSize, HalfSize, 0, 1);
			m_vertexData[quad * 4 + 1].Colour = particle->Colour;

			m_vertexData[quad * 4 + 2].Position = vec4(-HalfSize, -HalfSize, 0, 1);
			m_vertexData[quad * 4 + 2].Colour = particle->Colour;

			m_vertexData[quad * 4 + 3].Position = vec4(HalfSize, -HalfSize, 0, 1);
			m_vertexData[quad * 4 + 3].Colour = particle->Colour;

			vec3 zAxis = glm::normalize(vec3(a_CameraTransform[3]) - particle->Position);
			vec3 xAxis = glm::cross(vec3(a_CameraTransform[1]),zAxis);
			vec3 yAxis = glm::cross(zAxis,xAxis);
			glm::mat4 billboard(vec4(xAxis, 0), vec4(yAxis, 0), vec4(zAxis, 0), vec4(0, 0, 0, 1));

			m_vertexData[quad * 4 + 0].Position = billboard*m_vertexData[quad * 4 + 0].Position + vec4(particle->Position, 0);
			m_vertexData[quad * 4 + 1].Position = billboard*m_vertexData[quad * 4 + 1].Position + vec4(particle->Position, 0);
			m_vertexData[quad * 4 + 2].Position = billboard*m_vertexData[quad * 4 + 2].Position + vec4(particle->Position, 0);
			m_vertexData[quad * 4 + 3].Position = billboard*m_vertexData[quad * 4 + 3].Position + vec4(particle->Position, 0);

			++quad;
		}

	}
}

void ParticleEmitter::Draw()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, m_firstDead * 4 * sizeof(ParticleVertex), m_vertexData);

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_firstDead * 6, GL_UNSIGNED_INT, 0);
}