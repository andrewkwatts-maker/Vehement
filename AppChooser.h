#pragma once
#include "Application.h"


enum Apps
{
	Blank,
	fDistanceFields,
};

class AppChooser
{
public:

	static Application* GetApp(Apps APP_ID);

	AppChooser();
	~AppChooser();
};

