#include "ComplexTut1a.h"
#include "Camera.h"
#include "Clock.h"
#include "Inputs.h"

ComplexTut1a::ComplexTut1a()
{
	//BoxAnimation.AddFrame(ANIM_Keyframe(glm::vec3(1, 2, 0), glm::quat(glm::vec3(0, 0, 0)), 3.0f));
	//BoxAnimation.AddFrame(ANIM_Keyframe(glm::vec3(2, 1, 0), glm::quat(glm::vec3(1, 0, 0)), 2.2f));
	//BoxAnimation.AddFrame(ANIM_Keyframe(glm::vec3(1,0, 0), glm::quat(glm::vec3(1, 2, 0)), 1.2f));

	float StepTime = 1.5f;

	//HIPS

	//firstStep
	HIP1.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, 2), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime));
	HIP2.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, 2), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime));
	HIP3.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, -2), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime));
	HIP4.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, -2), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime));
	//secoundStep
	HIP1.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, 2), glm::quat(glm::vec3(-0.5f, 0, 0)), StepTime/2));
	HIP2.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, 2), glm::quat(glm::vec3(-0.5f, 0, 0)), StepTime/2));
	HIP3.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, -2), glm::quat(glm::vec3(-0.5f, 0, 0)), StepTime/2));
	HIP4.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, -2), glm::quat(glm::vec3(-0.5f, 0, 0)), StepTime/2));
	//ThirdStep
	HIP1.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, 2), glm::quat(glm::vec3(0.8f, 0, 0)), StepTime/4));
	HIP2.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, 2), glm::quat(glm::vec3(0.8f, 0, 0)), StepTime/4));
	HIP3.AddFrame(ANIM_Keyframe(glm::vec3(1, 4, -2), glm::quat(glm::vec3(0.8f, 0, 0)), StepTime/4));
	HIP4.AddFrame(ANIM_Keyframe(glm::vec3(-1, 4, -2), glm::quat(glm::vec3(0.8f, 0, 0)), StepTime/4));


	//KNEES

	//firstStep
	Knee1.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Knee2.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Knee3.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Knee4.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	//secoundStep
	Knee1.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.0f, 0, 0)), StepTime/2));
	Knee2.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.0f, 0, 0)), StepTime/2));
	Knee3.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.0f, 0, 0)), StepTime/2));
	Knee4.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.0f, 0, 0)), StepTime/2));
	//ThirdStep
	Knee1.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.5f, 0, 0)), StepTime/4));
	Knee2.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.5f, 0, 0)), StepTime/4));
	Knee3.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.5f, 0, 0)), StepTime/4));
	Knee4.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(-1.5f, 0, 0)), StepTime/4));

	//FEET

	//firstStep
	Foot1.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Foot2.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Foot3.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	Foot4.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.0f, 0, 0)), StepTime));
	//secoundStep/ThirdStep
	Foot1.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime*3/4));
	Foot2.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime*3/4));
	Foot3.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime*3/4));
	Foot4.AddFrame(ANIM_Keyframe(glm::vec3(0, -2, 0), glm::quat(glm::vec3(0.5f, 0, 0)), StepTime*3/4));


	//TimeOffset
	HIP2.Time = StepTime;
	HIP3.Time = StepTime;
	Knee2.Time = StepTime;
	Knee3.Time = StepTime;
	Foot2.Time = StepTime;
	Foot3.Time = StepTime;
}

bool ComplexTut1a::update()
{
	if (Application::update())
	{
		return true;
	}
	return false;
}

void ComplexTut1a::draw()
{
	//if (BoxAnimation.Update(0.001))
	//{
	//	Gizmos::addAABBFilled(glm::vec3(BoxAnimation.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(1), glm::vec4(1,0,1,1), &BoxAnimation.GetFrame());
	//}

	HIP1.Update(appBasics->AppClock->GetDelta());
	HIP2.Update(appBasics->AppClock->GetDelta());
	HIP3.Update(appBasics->AppClock->GetDelta());
	HIP4.Update(appBasics->AppClock->GetDelta());

	Knee1.Update(appBasics->AppClock->GetDelta());
	Knee2.Update(appBasics->AppClock->GetDelta());
	Knee3.Update(appBasics->AppClock->GetDelta());
	Knee4.Update(appBasics->AppClock->GetDelta());

	Foot1.Update(appBasics->AppClock->GetDelta());
	Foot2.Update(appBasics->AppClock->GetDelta());
	Foot3.Update(appBasics->AppClock->GetDelta());
	Foot4.Update(appBasics->AppClock->GetDelta());


	Gizmos::addAABBFilled(glm::vec3(HIP1.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &HIP1.GetFrame());
	Gizmos::addAABBFilled(glm::vec3(HIP2.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &HIP2.GetFrame());
	Gizmos::addAABBFilled(glm::vec3(HIP3.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &HIP3.GetFrame());
	Gizmos::addAABBFilled(glm::vec3(HIP4.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &HIP4.GetFrame());

	Gizmos::addAABBFilled(glm::vec3(HIP1.GetFrame()*Knee1.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP1.GetFrame()*Knee1.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP2.GetFrame()*Knee2.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP2.GetFrame()*Knee2.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP3.GetFrame()*Knee3.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP3.GetFrame()*Knee3.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP4.GetFrame()*Knee4.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP4.GetFrame()*Knee4.GetFrame()));

	Gizmos::addAABBFilled(glm::vec3(HIP1.GetFrame()*Knee1.GetFrame()*Foot1.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP1.GetFrame()*Knee1.GetFrame()*Foot1.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP2.GetFrame()*Knee2.GetFrame()*Foot2.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP2.GetFrame()*Knee2.GetFrame()*Foot2.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP3.GetFrame()*Knee3.GetFrame()*Foot3.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP3.GetFrame()*Knee3.GetFrame()*Foot3.GetFrame()));
	Gizmos::addAABBFilled(glm::vec3(HIP4.GetFrame()*Knee4.GetFrame()*Foot4.GetFrame()*glm::vec4(0, 0, 0, 1)), glm::vec3(0.2f), glm::vec4(1, 0, 1, 1), &(HIP4.GetFrame()*Knee4.GetFrame()*Foot4.GetFrame()));

	Gizmos::addAABBFilled(glm::vec3(0,4,0), glm::vec3(0.98f,0.75f,3), glm::vec4(1, 0, 1, 1),nullptr);
	Gizmos::addAABBFilled(glm::vec3(0, 5, -3.5f), glm::vec3(0.5f, 0.5f, 0.7f), glm::vec4(1, 0, 1, 1), nullptr);


	Application::draw();
}


ComplexTut1a::~ComplexTut1a()
{
}
