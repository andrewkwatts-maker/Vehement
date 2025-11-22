#pragma once
#include "Application.h"
#include <Windows.h>
#include "CL\cl.h"

class ComplexTut4:public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;


	cl_platform_id m_platform;
	cl_device_id m_device;
	cl_context m_context;
	cl_command_queue m_queue;
	cl_program m_program;
	cl_kernel m_kernel;
	cl_mem m_buffer;



	static const int VECTOR_COUNT = 5000000;
	glm::vec4 m_Vectors[VECTOR_COUNT];

	FILE* file;

	LARGE_INTEGER perfFreq;
	LARGE_INTEGER startTime, endTime;

	void StartClock();
	double EndClock();

	ComplexTut4();
	~ComplexTut4();
};

