#include "Inputs.h"

Inputs::Inputs()
{

}


void Inputs::Initualize_Inputs(AppBasics* AppData)
{
	WindowID = AppData->window;

	glfwGetCursorPos(WindowID, &dMouseX, &dMouseY);
	dProgram_MouseX = 0; //Initual Mouse Location;
	dProgram_MouseY = 0; //Initual Mouse Location;
	for (int i = 0; i<GLFW_KEY_LAST; i++)
	{
		iInputRefrances[i] = GLFW_KEY_UNKNOWN;
	}

	//LIST THE KEYS YOUR PROGRAM INTENDS TO USE
	//=========================================
	//Numbers
	ActivateKey(GLFW_KEY_0);
	ActivateKey(GLFW_KEY_1);
	ActivateKey(GLFW_KEY_2);
	ActivateKey(GLFW_KEY_3);
	ActivateKey(GLFW_KEY_4);
	ActivateKey(GLFW_KEY_5);
	ActivateKey(GLFW_KEY_6);
	ActivateKey(GLFW_KEY_7);
	ActivateKey(GLFW_KEY_8);
	ActivateKey(GLFW_KEY_9);

	//WASD
	ActivateKey(GLFW_KEY_W);
	ActivateKey(GLFW_KEY_A);
	ActivateKey(GLFW_KEY_S);
	ActivateKey(GLFW_KEY_D);

	//Arrows
	ActivateKey(GLFW_KEY_UP);
	ActivateKey(GLFW_KEY_DOWN);
	ActivateKey(GLFW_KEY_LEFT);
	ActivateKey(GLFW_KEY_RIGHT);

	//Enter
	ActivateKey(GLFW_KEY_ENTER);

	//Others
	ActivateKey(GLFW_KEY_Z);
	ActivateKey(GLFW_KEY_X);
	ActivateKey(GLFW_KEY_C);
	ActivateKey(GLFW_KEY_V);
	ActivateKey(GLFW_KEY_B);
	ActivateKey(GLFW_KEY_N);
	ActivateKey(GLFW_KEY_M);
	ActivateKey(GLFW_KEY_V);

	ActivateKey(GLFW_KEY_LEFT_SHIFT);

	//Delete
	ActivateKey(GLFW_KEY_DELETE);



	//LIST THE KEYS YOUR PROGRAM INTENDS TO USE END
	//=============================================
};

bool Inputs::ActivateKey(int Key_enum)
{
	KeyPress KeyTemplate;
	KeyTemplate.GLFW_RefranceNumber = Key_enum;
	if (Key_enum > GLFW_KEY_UNKNOWN && Key_enum< GLFW_KEY_LAST && iInputRefrances[Key_enum] == GLFW_KEY_UNKNOWN)
	{
		iInputRefrances[Key_enum] = kKEYARRAY.size();
		kKEYARRAY.emplace_back(KeyTemplate);
		return true;
	}
	else
	{
		return false; //key enum out of range or already active;
	}
};

bool Inputs::IsKeyDown(int Key_enum)
{
	if (iInputRefrances[Key_enum] != GLFW_KEY_UNKNOWN)
	{
		return kKEYARRAY[iInputRefrances[Key_enum]].isKeyDown;
	}
	else if (Key_enum > GLFW_KEY_UNKNOWN && Key_enum < iNumberButtonsOnMouse)
	{
		return IsMouseDown(Key_enum);
	}
	else
	{
		return false;
	}
};
bool Inputs::KeyPressed(int Key_enum)
{
	if (iInputRefrances[Key_enum] != GLFW_KEY_UNKNOWN)
	{
		return kKEYARRAY[iInputRefrances[Key_enum]].KeyPressed;
	}
	else if (Key_enum > GLFW_KEY_UNKNOWN && Key_enum < iNumberButtonsOnMouse)
	{
		return MousePressed(Key_enum);
	}
	else
	{
		return false;
	}
};
bool Inputs::KeyReleased(int Key_enum)
{
	if (iInputRefrances[Key_enum] != GLFW_KEY_UNKNOWN)
	{
		return kKEYARRAY[iInputRefrances[Key_enum]].KeyReleased;
	}
	else if (Key_enum > GLFW_KEY_UNKNOWN && Key_enum < iNumberButtonsOnMouse)
	{
		return MouseReleased(Key_enum);
	}
	else
	{
		return false;
	}
};

bool Inputs::IsMouseDown(int Key_enum)
{
	return MouseButtons[Key_enum].isKeyDown;
};

bool Inputs::MousePressed(int Key_enum)
{
	return MouseButtons[Key_enum].KeyPressed;
};

bool Inputs::MouseReleased(int Key_enum)
{
	return MouseButtons[Key_enum].KeyReleased;
};

void Inputs::UpdateInputs(AppBasics* AppData)
{
	//KeyStrokes
	for (int ActiveKeyNumber = 0; ActiveKeyNumber<kKEYARRAY.size(); ActiveKeyNumber++)
	{
		kKEYARRAY[ActiveKeyNumber].Last_isKeyDown = kKEYARRAY[ActiveKeyNumber].isKeyDown;
		kKEYARRAY[ActiveKeyNumber].isKeyDown = glfwGetKey(WindowID, kKEYARRAY[ActiveKeyNumber].GLFW_RefranceNumber);

		if (kKEYARRAY[ActiveKeyNumber].Last_isKeyDown == false && kKEYARRAY[ActiveKeyNumber].isKeyDown == true)
		{
			kKEYARRAY[ActiveKeyNumber].KeyPressed = true;
		}
		else
		{
			kKEYARRAY[ActiveKeyNumber].KeyPressed = false;
		}
		if (kKEYARRAY[ActiveKeyNumber].Last_isKeyDown == true && kKEYARRAY[ActiveKeyNumber].isKeyDown == false)
		{
			kKEYARRAY[ActiveKeyNumber].KeyReleased = true;
		}
		else
		{
			kKEYARRAY[ActiveKeyNumber].KeyReleased = false;
		}
	}

	//Mouse Location Calculations

	dLAST_mouseX = dMouseX;
	dLAST_mouseY = dMouseY;
	glfwGetCursorPos(WindowID, &dMouseX, &dMouseY);
	dMouseXDelta = dMouseX - dLAST_mouseX;
	dMouseYDelta = dMouseY - dLAST_mouseY;
	dProgram_MouseX += dMouseXDelta;
	dProgram_MouseY += dMouseYDelta;

	if (dProgram_MouseX < 0)
	{
		dProgram_MouseX = 0;
	}
	if (dProgram_MouseY < 0)
	{
		dProgram_MouseY = 0;
	}
	if (dProgram_MouseX > AppData->ScreenSize.x)
	{
		dProgram_MouseX = AppData->ScreenSize.x;
	}
	if (dProgram_MouseY > AppData->ScreenSize.y)
	{
		dProgram_MouseY = AppData->ScreenSize.y;
	}

	//Mouse Button Presses
	for (int Button = 0; Button<iNumberButtonsOnMouse; Button++)
	{
		MouseButtons[Button].Last_isKeyDown = MouseButtons[Button].isKeyDown;
		MouseButtons[Button].isKeyDown = glfwGetMouseButton(WindowID, Button);

		if (MouseButtons[Button].Last_isKeyDown == false && MouseButtons[Button].isKeyDown == true)
		{
			MouseButtons[Button].KeyPressed = true;
		}
		else
		{
			MouseButtons[Button].KeyPressed = false;
		}
		if (MouseButtons[Button].Last_isKeyDown == true && MouseButtons[Button].isKeyDown == false)
		{
			MouseButtons[Button].KeyReleased = true;
		}
		else
		{
			MouseButtons[Button].KeyReleased = false;
		}
	}

};

vec2 Inputs::MouseLoc()
{
	return vec2(dMouseX, dMouseY);
}

Inputs::~Inputs()
{
}
