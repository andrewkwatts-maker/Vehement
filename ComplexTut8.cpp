#include "ComplexTut8.h"
#include "glm\glm.hpp"
#include "Clock.h"

Fuzzy fuzzyEngine;

ComplexTut8::ComplexTut8()
{
}


ComplexTut8::~ComplexTut8()
{
}



bool ComplexTut8::update()
{
	if (Application::update())
	{
		worldController->update(appBasics->AppClock->GetDelta());
		return true;
	}
	else
	{
		return false;
	}

}
void ComplexTut8::draw()
{
	worldController->draw();
	Application::draw();
}
bool ComplexTut8::startup()
{
	if (Application::startup())
	{
		FuzzyLogicExample();
		return true;
	}
	else
	{
		return false;
	}
}


void ComplexTut8::FuzzyLogicExample()
{
		//set up simulation world.
	worldController = new WorldController();
	Agent* simpleAI = new Agent(glm::vec2(500, 400));
	worldController->addObject(simpleAI);
	Water* water1 = new Water(glm::vec2(1000, 100));
	worldController->addObject(water1);
	Cave* cave = new Cave(glm::vec2(200, 100));
	worldController->addObject(cave);
	Food* food = new Food(glm::vec2(300, 600));
	worldController->addObject(food);

	//the following code sets up all the membership functions for the fuzzy sets

	//membership functions for the tiredess set
	fuzzyEngine.tired = new leftShoulderMembershipFunction(0.2f, 0.4f, "tired");
	fuzzyEngine.awake = new TrapezoidFunction(0.2f, 0.4f, 0.6f, 0.8f, "awake");
	fuzzyEngine.superActive = new rightShoulderMembershipFunction(0.6f, 0.8f, "SuperActive");

	//membership functions for the hunger set
	fuzzyEngine.veryHungry = new leftShoulderMembershipFunction(0.2f, 0.4f, "very hungry");
	fuzzyEngine.hungry = new TrapezoidFunction(.2f, .4f, .8f, .9f, "hungry");
	fuzzyEngine.full = new rightShoulderMembershipFunction(.8f, .9f, "full");

	//membership functions for the thirst set
	fuzzyEngine.WeekFromThirsty = new leftShoulderMembershipFunction(0.1f, 0.2f, "week from thirst");
	fuzzyEngine.veryThirsty = new TriangleFunction(0.1f, 0.2f, 0.3f, "very thristy");
	fuzzyEngine.thirsty = new TriangleFunction(0.2f, 0.4f, 0.6f, "thristy");
	fuzzyEngine.notThirsty = new rightShoulderMembershipFunction(.4f, .6f, "not thirsty");

	//membership functions for the distance set
	fuzzyEngine.veryNear = new leftShoulderMembershipFunction(2, 4, "very close");
	fuzzyEngine.mediumRange = new TrapezoidFunction(2, 4, 50, 70, "medium range");
	fuzzyEngine.farAway = new rightShoulderMembershipFunction(50, 70, "far away");

	//membership functions for the desirability set (used for defuzification)
	fuzzyEngine.undesirable = new leftShoulderMembershipFunction(0.3f, 0.5f, "undesirable");
	fuzzyEngine.desirable = new TriangleFunction(0.3f, 0.5f, 0.7f, "desirable");
	fuzzyEngine.veryDesirable = new rightShoulderMembershipFunction(0.5f, 0.7f, "very desirable");
}