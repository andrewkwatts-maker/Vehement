#include "AppChooser.h"
//define STB_IMAGE_IMPLEMENTATION - Needed if not using fbx loader


int main()
{
	//Application* App = AppChooser::GetApp(GraphicsTutorial6a);

	//Application* App = AppChooser::GetApp(VoroExample3);
	//Application* App = AppChooser::GetApp(GraphicsTutorial6);
	//Application* App = AppChooser::GetApp(ComplexTutorial5);
	//Application* App = AppChooser::GetApp(GraphicsAssigment1);
	Application* App = AppChooser::GetApp(IndustryShowcase1);
	//Application* App = AppChooser::GetApp(ComplexAssigment1);

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