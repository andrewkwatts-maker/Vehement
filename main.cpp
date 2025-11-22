#include "AppChooser.h"
//define STB_IMAGE_IMPLEMENTATION - Needed if not using fbx loader


int main()
{

	//Application* App = AppChooser::GetApp(fDistanceFields);
	Application* App = AppChooser::GetApp(Blank);

	if (App->startup() == true)
	{
		while (App->update() == true)
		{
			App->drawBegin();
			App->draw();
			App->drawEnd();
		}
		App->shutdown();
	}
	delete App;
	return 0;
}