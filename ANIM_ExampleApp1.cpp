#include "ANIM_ExampleApp1.h"
#include "Camera.h"
#include "Clock.h"
#include "Inputs.h"
#include "ANIM.h"

enum BodyParts
{
	Hip1,
	Hip2,
	Hip3,
	Hip4,
	Knee1,
	Knee2,
	Knee3,
	Knee4,
	Foot1,
	Foot2,
	Foot3,
	Foot4,
	END_PARTS,

};

ANIM_ExampleApp1::ANIM_ExampleApp1()
{
	WheelType FrameA; //0.2   Pos
	WheelType FrameB; //0.5  Pos
	WheelType FrameC; //0.8  Pos

	INF_LOOP_RESULT ResultTest;

	ANIM_Frame New(vec3(0),quat(0,0,0,0)); //Filler
	
	for (int i = 0; i < END_PARTS; i++)
	{
		FrameA.push_back(New);
		FrameB.push_back(New);
		FrameC.push_back(New);
	}
	
	//Hips
	FrameA[Hip1] = ANIM_Frame(glm::vec3(1.7f, 3.0f, 3), glm::quat(glm::vec3(0.4f, 0, 0)));
	FrameA[Hip2] = ANIM_Frame(glm::vec3(-1.7f, 3.3f, 3), glm::quat(glm::vec3(-0.4f, 0, 0)));
	FrameA[Hip3] = ANIM_Frame(glm::vec3(1.7f, 3.3f, -3), glm::quat(glm::vec3(-0.4f, 0, 0)));
	FrameA[Hip4] = ANIM_Frame(glm::vec3(-1.7f, 3.0f, -3), glm::quat(glm::vec3(0.4f, 0, 0)));
	
	FrameB[Hip1] = ANIM_Frame(glm::vec3(1.7f, 3.3f, 3), glm::quat(glm::vec3(-0.4f, 0, 0)));
	FrameB[Hip2] = ANIM_Frame(glm::vec3(-1.7f, 3.0f, 3), glm::quat(glm::vec3(1.0f, 0, 0)));
	FrameB[Hip3] = ANIM_Frame(glm::vec3(1.7f, 3.0f, -3), glm::quat(glm::vec3(1.0f, 0, 0)));
	FrameB[Hip4] = ANIM_Frame(glm::vec3(-1.7f, 3.3f, -3), glm::quat(glm::vec3(-0.4f, 0, 0)));

	FrameC[Hip1] = ANIM_Frame(glm::vec3(1.7f, 3, 3), glm::quat(glm::vec3(1.0f, 0, 0)));
	FrameC[Hip2] = ANIM_Frame(glm::vec3(-1.7f, 3, 3), glm::quat(glm::vec3(0.5f, 0, 0)));
	FrameC[Hip3] = ANIM_Frame(glm::vec3(1.7f, 3, -3), glm::quat(glm::vec3(0.5f, 0, 0)));
	FrameC[Hip4] = ANIM_Frame(glm::vec3(-1.7f, 3, -3), glm::quat(glm::vec3(1.0f, 0, 0)));
	
	//Knees
	FrameA[Knee1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameA[Knee2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.0f, 0, 0)));
	FrameA[Knee3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.0f, 0, 0)));
	FrameA[Knee4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
																					 
	FrameB[Knee1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.0f, 0, 0)));
	FrameB[Knee2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.3f, 0, 0)));
	FrameB[Knee3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.3f, 0, 0)));
	FrameB[Knee4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.0f, 0, 0)));

	FrameC[Knee1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.3f, 0, 0)));
	FrameC[Knee2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-0.4f, 0, 0)));
	FrameC[Knee3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-0.4f, 0, 0)));
	FrameC[Knee4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(-1.3f, 0, 0)));
	
	//Feet
	FrameA[Foot1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameA[Foot2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.5f, 0, 0)));
	FrameA[Foot3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.5f, 0, 0)));
	FrameA[Foot4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
		   
	FrameB[Foot1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.5f, 0, 0)));
	FrameB[Foot2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameB[Foot3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameB[Foot4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.5f, 0, 0)));

	FrameC[Foot1] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameC[Foot2] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameC[Foot3] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	FrameC[Foot4] = ANIM_Frame(glm::vec3(0, -1.8f, 0), glm::quat(glm::vec3(0.0f, 0, 0)));
	
	//AnimationWheel.AddAsset(FrameA, 0.1f);
	//AnimationWheel.AddAsset(FrameB, 0.5f);
	//AnimationWheel.AddAsset(FrameC, 0.8f);

	AnimationWheel.AddAsset(FrameA, 0.1f);
	AnimationWheel.AddAsset(FrameB, 0.5f);
	AnimationWheel.AddAsset(FrameC, 0.8f);




}

bool ANIM_ExampleApp1::update()
{
	if (Application::update())
	{
		
		return true;
	}
	return false;
}


void ANIM_ExampleApp1::draw()
{
	MochLoc += vec3(27)*appBasics->AppClock->GetDelta();
	auto Results = AnimationWheel.Update(MochLoc, 5);
	//auto ResultsVector = ANIM_FUNCTS::CompileResults(AnimationWheel.GetSmoothedResults(Results));
	//auto ResultsVector = ANIM_FUNCTS::CompileResults(ANIM_FUNCTS::LerpResults(Results.Next, Results.Previous, Results.LERP_Ratio));

	std::vector<mat4> ResultsVector;
	for (int i = 0; i < END_PARTS; i++)
	{
		//ResultsVector.emplace_back(LerpResults(Results.Next[i].Position, Results.Next[i].Rotation, Results.Previous[i].Position, Results.Previous[i].Rotation, Results.LERP_Ratio));
	
		ResultsVector.emplace_back(SmoothLerpResults(Results.Next[i].Position, Results.Next[i].Rotation, Results.Next2[i].Position, Results.Next2[i].Rotation, Results.Previous[i].Position, Results.Previous[i].Rotation, Results.Previous2[i].Position, Results.Previous2[i].Rotation, Results.LERP_Ratio));
		//ResultsVector.emplace_back(CardinalLerpResults(Results.Next[i].Position, Results.Next[i].Rotation, Results.Next2[i].Position, Results.Next2[i].Rotation, Results.Previous[i].Position, Results.Previous[i].Rotation, Results.Previous2[i].Position, Results.Previous2[i].Rotation,2.0f, Results.LERP_Ratio));
	}

	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip1] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &ResultsVector[Hip1]);
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip2] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &ResultsVector[Hip2]);
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip3] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &ResultsVector[Hip3]);
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip4] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &ResultsVector[Hip4]);

	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip1] * ResultsVector[Knee1] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip1] * ResultsVector[Knee1]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip2] * ResultsVector[Knee2] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip2] * ResultsVector[Knee2]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip3] * ResultsVector[Knee3] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip3] * ResultsVector[Knee3]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip4] * ResultsVector[Knee4] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip4] * ResultsVector[Knee4]));

	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip1] * ResultsVector[Knee1] * ResultsVector[Foot1] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip1] * ResultsVector[Knee1] * ResultsVector[Foot1]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip2] * ResultsVector[Knee2] * ResultsVector[Foot2] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip2] * ResultsVector[Knee2] * ResultsVector[Foot2]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip3] * ResultsVector[Knee3] * ResultsVector[Foot3] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip3] * ResultsVector[Knee3] * ResultsVector[Foot3]));
	Gizmos::addAABBFilled(glm::vec3(ResultsVector[Hip4] * ResultsVector[Knee4] * ResultsVector[Foot4] * glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(ResultsVector[Hip4] * ResultsVector[Knee4] * ResultsVector[Foot4]));

	Gizmos::addAABBFilled(glm::vec3(0, 3.3f, 0), glm::vec3(1.3f, 0.75f, 4), glm::vec4(1, 0, 1, 1), nullptr);
	Gizmos::addAABBFilled(glm::vec3(0, 5, -4.5f), glm::vec3(0.7f, 0.5f, 1.2f), glm::vec4(1, 0, 1, 1), nullptr);

	Application::draw();
}




ANIM_ExampleApp1::~ANIM_ExampleApp1()
{
}
