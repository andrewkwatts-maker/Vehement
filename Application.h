#pragma once
#include <aie\Gizmos.h>
#include <glm\glm.hpp>
//MyClasses


using glm::mat4;
using glm::vec2;

//predefines
struct GLFWwindow;
class Clock;
class Camera;
class Inputs;
class GL_Manager;

class AppBasics
{
public:
	GLFWwindow* window;
	vec2 ScreenSize;
	Camera* AppCamera;
	Clock* AppClock;
	Inputs* AppInputs;

	AppBasics();
	~AppBasics();
};

class Application
{
public:

	virtual bool startup();
	virtual bool update();
	virtual void draw();
	virtual void drawBegin();
	virtual void drawEnd();
	virtual void shutdown();

	bool RunDrawBeginAndEnd = true;
	AppBasics* appBasics;
	GL_Manager* OGL_Manager;
	
	Application();
	~Application();

private:
	void Grid();

};

