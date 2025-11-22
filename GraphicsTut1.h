#pragma once
#include "Application.h"
class GraphicsTut1:public Application
{
public:

	glm::vec3 EarthPosition;
	glm::vec3 MoonPosition;

	bool update() override;
	void draw() override;
	void RunSolarSystem();

	GraphicsTut1();
	~GraphicsTut1();
};

