#pragma once
#include "Application.h"


enum Apps
{
	Blank,
	AnimationExample1,
	VoroExample3,
	GraphicsAssigment1,
	ComplexAssigment1,
	IndustryShowcase1,

	ComplexTutorial1,
	ComplexTutorial1a,
	ComplexTutorial2,
	ComplexTutorial3,
	ComplexTutorial4,
	ComplexTutorial5,
	ComplexTutorial6,
	ComplexTutorial8,
	ComplexTutorial11,
	ComplexTutorial11a,

	GraphicsTutorial1,
	GraphicsTutorial3,
	GraphicsTutorial4,
	GraphicsTutorial5,
	GraphicsTutorial6,
	GraphicsTutorial6a,
	GraphicsTutorial7,
	GraphicsTutorial8,
	GraphicsTutorial9,
	GraphicsTutorial10,
	GraphicsTutorial11,
	GraphicsTutorial12,
	GraphicsTutorial13,
	GraphicsTutorial14,
};

class AppChooser
{
public:

	static Application* GetApp(Apps APP_ID);

	AppChooser();
	~AppChooser();
};

