#pragma once
#include "Application.h"
#include "GPU_ParticleEmitter.h"
class GraphicsTut10 :
	public Application
{
public:
	GPU_ParticleEmitter* Emitter;

	bool update() override;
	void draw() override;
	bool startup() override;

	GraphicsTut10();
	~GraphicsTut10();
};

