#pragma once
#include "Application.h"
class GraphicsTut11 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;
	GraphicsTut11();
	~GraphicsTut11();
};

