#pragma once
#include "Application.h"
class GraphicsTut12 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	int ShaderProgram;
	int FrameBuffer;
	int RenderTarget;

	int Mirror;

	GraphicsTut12();
	~GraphicsTut12();
};

