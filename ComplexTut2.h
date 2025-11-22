#pragma once
#include "Application.h"

class ComplexTut2:public Application
{
public:
	bool update() override;
	void draw() override;

	static void printText(int i);

	ComplexTut2();
	~ComplexTut2();
};

