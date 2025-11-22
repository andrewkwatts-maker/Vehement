#pragma once
#include "Application.h"
#include "ANIM.h"
#include <vector>

class ANIM_ExampleApp1 : public Application
{
public:

	bool update() override;
	void draw() override;

	vec3 MochLoc;
	ANIM_Wheel AnimationWheel;

	ANIM_ExampleApp1();
	~ANIM_ExampleApp1();
};

