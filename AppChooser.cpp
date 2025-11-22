#include "AppChooser.h"
#include "fDisField.h"

AppChooser::AppChooser()
{
}

Application* AppChooser::GetApp(Apps APP_ID)
{
	switch (APP_ID)
	{
		//Empty 3d Applicaiton wGrid;
	case Blank:
		return new Application;
	case fDistanceFields:
		return new fDisField;


		//Default
	default:
		return new Application;
		break;
	}
};


AppChooser::~AppChooser()
{
}
