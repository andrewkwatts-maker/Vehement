#include "GraphicsTut11.h"


GraphicsTut11::GraphicsTut11()
{
}


GraphicsTut11::~GraphicsTut11()
{
}

bool GraphicsTut11::update()
{
	if (Application::update())
	{
		return true;
	}
	else
	{
		return false;
	}

}
void GraphicsTut11::draw()
{

	Application::draw();
}


bool GraphicsTut11::startup()
{
	if (Application::startup())
	{

		return true;
	}
	else
	{
		return false;
	}
}
