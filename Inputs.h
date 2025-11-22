#pragma once
#include <vector>
#include "Application.h"
#include <glfw\glfw3.h>

//Input Key Structures
//============================
struct KeyPress
{
public:
	bool isKeyDown;
	bool KeyPressed;
	bool KeyReleased;
	bool Last_isKeyDown;
	int GLFW_RefranceNumber;
	KeyPress()
	{
		Last_isKeyDown = false;
		isKeyDown = false;
		KeyPressed = false;
		KeyReleased = false;

	}
};


class Inputs
{
public:

	void Initualize_Inputs(AppBasics* AppData);
	void UpdateInputs(AppBasics* AppData);
	bool ActivateKey(int Key_enum);
	bool IsKeyDown(int Key_enum);
	bool KeyPressed(int Key_enum);
	bool KeyReleased(int Key_enum);

	bool IsMouseDown(int Key_enum);
	bool MousePressed(int Key_enum);
	bool MouseReleased(int Key_enum);
	vec2 MouseLoc();

	int iInputRefrances[348];
	std::vector<KeyPress> kKEYARRAY;

	int const static iNumberButtonsOnMouse = 2;
	KeyPress MouseButtons[iNumberButtonsOnMouse];

	GLFWwindow* WindowID;
	double dProgram_MouseX; //mouse location confined to window
	double dProgram_MouseY; //mouse location confined to window
	double dMouseX;
	double dMouseY;
	double dLAST_mouseX;
	double dLAST_mouseY;
	double dMouseXDelta;
	double dMouseYDelta;

	Inputs();
	~Inputs();
};

