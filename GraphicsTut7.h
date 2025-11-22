#pragma once
#include "Application.h"
class GraphicsTut7 :
	public Application
{
public:

	bool update() override;
	void draw() override;
	bool startup() override;

	int PyroModel;
	int Point_Textured_Bump;
	float Time;
	GraphicsTut7();
	~GraphicsTut7();
};

