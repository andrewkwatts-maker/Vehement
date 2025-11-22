#pragma once

struct Struct_Clock
{
	double Second;
	int Minute;
	int Hour;
	int Day;
};

class Clock
{
public:

	//TIME Functions
	//===========================
	double RunClock();
	void CalibrateClock();
	Struct_Clock GetProgramTime();
	double GetDelta();
	void ResetClock();
	void PrintClock();

	Clock();
	~Clock();

private:
	double CurrentTime; //DO NOT ADJUST - READ ONLY
	double PreviousTime; //DO NOT ADJUST - READ ONLY
	double DeltaTime; //DO NOT ADJUST - READ ONLY
	Struct_Clock Program_Clock; //DO NOT ADJUST - READ ONLY

};

