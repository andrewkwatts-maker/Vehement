#include "Clock.h"
#include "glfw\glfw3.h"
#include <stdio.h>

Clock::Clock()
{
}

void Clock::PrintClock()
{
	printf("Day:%i Hour:%i Min:%i Sec:%f DT:%f FPS:%i \n", Program_Clock.Day, Program_Clock.Hour, Program_Clock.Minute, Program_Clock.Second,DeltaTime,(int)(1.f/DeltaTime));
}

double Clock::GetDelta()
{
	return DeltaTime;
}

double Clock::RunClock() //Also adds time to the Clock Stored in "ProgramRunTime";
{
	//UpdateDelta
	PreviousTime = CurrentTime;
	CurrentTime = glfwGetTime();
	DeltaTime = CurrentTime - PreviousTime;

	//UpdateClock
	Program_Clock.Second += DeltaTime;
	while (Program_Clock.Second >= 60) //WhileLoops used to catch LargeDeltaSpikes - More then likely unneccassary, but low cost safety net.
	{
		Program_Clock.Second -= 60;
		Program_Clock.Minute++;
		while (Program_Clock.Minute >= 60)
		{
			Program_Clock.Minute -= 60;
			Program_Clock.Hour++;
			while (Program_Clock.Hour >= 24)
			{
				Program_Clock.Hour -= 24;
				Program_Clock.Day++;
			}
		}
	}

	return DeltaTime;
}
void Clock::CalibrateClock() //ONLY USED WHEN INITIATING A PROGRAM
{
	ResetClock();
	CurrentTime = glfwGetTime();
	PreviousTime = CurrentTime;
}
Struct_Clock Clock::GetProgramTime()
{
	return Program_Clock;
};
void Clock::ResetClock()
{
	CurrentTime = 0;
	PreviousTime = 0;
	Program_Clock.Day = 0;
	Program_Clock.Hour = 0;
	Program_Clock.Minute = 0;
	Program_Clock.Second = 0;
	DeltaTime = 0;
};


Clock::~Clock()
{
}
