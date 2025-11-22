#pragma once
#include "Application.h"
#include "ANIM.h"

class ComplexTut1a: public Application
{
public:

	ANIM_Sequence BoxAnimation;

	ANIM_Sequence HIP1;
	ANIM_Sequence HIP2;
	ANIM_Sequence HIP3;
	ANIM_Sequence HIP4;

	ANIM_Sequence Knee1;
	ANIM_Sequence Knee2;
	ANIM_Sequence Knee3;
	ANIM_Sequence Knee4;

	ANIM_Sequence Foot1;
	ANIM_Sequence Foot2;
	ANIM_Sequence Foot3;
	ANIM_Sequence Foot4;


	bool update() override;
	void draw() override;
	ComplexTut1a();
	~ComplexTut1a();
};

