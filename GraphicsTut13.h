#pragma once
#include "Application.h"
class GraphicsTut13 :
	public Application
{
public:

	bool update() override;
	void draw() override;
	bool startup() override;

	int ShaderProgram;
	int FrameBuffer;
	int RenderTarget1;
	int RenderTarget2;

	int Screen;

	GraphicsTut13();
	~GraphicsTut13();
};

