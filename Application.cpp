#include "Application.h"


//RawIncludes
#include <LoadGen/gl_core_4_4.h>
#include <iostream>

//AdditionalIncludes
#include <glm\ext.hpp>
#include <glfw/glfw3.h> //MustbeAfter GLMcalls
#include "AntTweakBar.h"

//myIncludes;
#include "Camera.h"
#include "Clock.h"
#include "Inputs.h"
#include "GL_Manager.h"

AppBasics::AppBasics()
{

}

AppBasics::~AppBasics()
{

}

Application::Application()
{
	appBasics = new AppBasics;
	OGL_Manager = new GL_Manager;

	//AppBasicsContent
	appBasics->AppCamera = new Camera;
	appBasics->AppClock = new Clock;
	appBasics->AppInputs = new Inputs;
}

void OnMouseButton(GLFWwindow*, int b, int a, int m) //AntTweak Menu 
{
	TwEventMouseButtonGLFW(b, a);
}

void OnMousePosition(GLFWwindow*, double X, double Y) //AntTweak Menu 
{
	TwEventMousePosGLFW((int)X, (int)Y);
}

void OnMouseScroll(GLFWwindow*, double X, double Y) //AntTweak Menu 
{
	TwEventMouseWheelGLFW((int)Y);
}

void OnKey(GLFWwindow*, int k, int s, int a, int m) //AntTweak Menu 
{
	TwEventKeyGLFW(k, a);
}

void OnChar(GLFWwindow*, unsigned int c) //AntTweak Menu 
{
	TwEventCharGLFW(c, GLFW_PRESS);
}

void OnWindowResize(GLFWwindow*, int w, int h) //AntTweak Menu 
{
	TwWindowSize(w, h);
	glViewport(0, 0, w, h);
}

bool Application::startup()
{
	appBasics->ScreenSize = vec2(1920, 1080);
	OGL_Manager->SetNullFrameData(appBasics->ScreenSize);

	int ErrorID = 0;

	//====================================================================================================
	//OGL Window Establishment Code
	//====================================================================================================

	if (glfwInit() == false)
	{
		ErrorID = - 1;
		return false;
	}

	appBasics->window = glfwCreateWindow((int)appBasics->ScreenSize.x, (int)appBasics->ScreenSize.y, "Window", nullptr, nullptr);

	if (appBasics->window == nullptr)
	{
		glfwTerminate();
		ErrorID = - 2;
		return false;
	}

	glfwMakeContextCurrent(appBasics->window);

	//====================================================================================================
	//OGL Window Establishment Code END
	//====================================================================================================


	//====================================================================================================
	//OGL Functions Establishment Code
	//====================================================================================================
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		glfwDestroyWindow(appBasics->window);
		glfwTerminate();
		ErrorID = - 3;
		return false;
	}

	auto major = ogl_GetMajorVersion();
	auto minor = ogl_GetMinorVersion();
	printf("GL: %i.%i\n", major, minor);

	//BackgroundColour
	glClearColor(0, 0, 0, 1);
	//Enable DepthBuffer
	glEnable(GL_DEPTH_TEST);
	//====================================================================================================
	//OGL Functions Establishment Code END
	//====================================================================================================

	//====================================================================================================
	//Establishment Code
	//====================================================================================================

	//Camera
	Gizmos::create();

	appBasics->AppCamera->SetPerspective(glm::pi<float>()*0.25f, appBasics->ScreenSize.x / appBasics->ScreenSize.y, 0.1f, 1000.0f);
	appBasics->AppCamera->SetupCamera(vec3(10, 10, 10), vec3(0), vec3(0, 1, 0));

	appBasics->AppClock->CalibrateClock();
	appBasics->AppInputs->Initualize_Inputs(appBasics);


	//AntTweak Menu Establishment
	TwInit(TW_OPENGL_CORE, nullptr);
	TwWindowSize((int)appBasics->ScreenSize.x, (int)appBasics->ScreenSize.y);
	glfwSetMouseButtonCallback(appBasics->window, OnMouseButton);
	glfwSetCursorPosCallback(appBasics->window, OnMousePosition);
	glfwSetScrollCallback(appBasics->window, OnMouseScroll);
	glfwSetKeyCallback(appBasics->window, OnKey);
	glfwSetCharCallback(appBasics->window, OnChar);
	glfwSetWindowSizeCallback(appBasics->window, OnWindowResize);

	//====================================================================================================
	//Establishment Code END
	//====================================================================================================



	return true;


};
bool Application::update()
{
	if (glfwWindowShouldClose(appBasics->window) == false && glfwGetKey(appBasics->window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
	{
		appBasics->AppClock->RunClock();
		appBasics->AppCamera->FlyCamera(appBasics);
		appBasics->AppInputs->UpdateInputs(appBasics);
		return true;
	}
	return false;
};
void Application::drawBegin()
{
	if (RunDrawBeginAndEnd)
	{
		//clear BackBuffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//clear Gizmos
		Gizmos::clear();
	}
};

void Application::drawEnd()
{
	if (RunDrawBeginAndEnd)
	{
		//gizmos draw
		Gizmos::draw(appBasics->AppCamera->GetProjectionView());
	}
		//EndLoopOGLEvents
	glfwSwapBuffers(appBasics->window);
	glfwPollEvents();
};

void Application::draw()
{
	Grid();
};
void Application::shutdown()
{
	//====================================================================================================
	//De-Establishment Code
	//====================================================================================================

	//Gizmos
	Gizmos::destroy();
	//====================================================================================================
	//De-Establishment Code End
	//====================================================================================================


	//====================================================================================================
	//OGL De-Establishment Code
	//====================================================================================================

	glfwDestroyWindow(appBasics->window);
	glfwTerminate();
	//====================================================================================================
	//OGL De-Establishment Code End
	//====================================================================================================
};

void Application::Grid()
{
	glm::vec4 white = glm::vec4(1);
	glm::vec4 purple = glm::vec4(0.5f, 0, 0.5f, 1);

	Gizmos::addTransform(glm::mat4(1));

	for (int i = 0; i < 21; ++i)
	{
		Gizmos::addLine(glm::vec3(-10 + i, 0, 10), glm::vec3(-10 + i, 0, -10), i == 10 ? white : purple);
		Gizmos::addLine(glm::vec3(10, 0, -10 + i), glm::vec3(-10, 0, -10 + i), i == 10 ? white : purple);
	}

}


Application::~Application()
{
	delete appBasics->AppCamera;
	delete appBasics->AppClock;
	delete appBasics->AppInputs;
	delete appBasics;
}
