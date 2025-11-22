#pragma once
#include "Application.h"
#include "ParticleSystem.h"
class GraphicsTut8 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;


	ParticleEmitter* ParticleSystem;
	int ParticleShaderProgram;

	GraphicsTut8();
	~GraphicsTut8();
};

