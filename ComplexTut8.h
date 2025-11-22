#pragma once
#include "Application.h"
#include "FuzzyLogic\FuzzyLogicEngine.h"
#include "FuzzyLogic\FuzzyLogicProject.h"
#include "FuzzyLogic\SimulationObjects.h"
class ComplexTut8 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;


	WorldController* worldController;

	bool GLFWMouseButton1Down;
	void FuzzyLogicExample();

	ComplexTut8();
	~ComplexTut8();
};

