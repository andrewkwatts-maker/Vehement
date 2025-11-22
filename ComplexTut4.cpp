#include "ComplexTut4.h"
#include <iosfwd>
#include <CL\opencl.h>
#include <CL\cl_gl.h>
#include <CL\cl_gl_ext.h>

ComplexTut4::ComplexTut4()
{
	
}


ComplexTut4::~ComplexTut4()
{
	clReleaseContext(m_context);
	clReleaseCommandQueue(m_queue);
	clReleaseProgram(m_program);
	clReleaseKernel(m_kernel);
	clReleaseMemObject(m_buffer);
}


void ComplexTut4::StartClock()
{
	QueryPerformanceCounter(&startTime);
};
double ComplexTut4::EndClock()
{
	QueryPerformanceCounter(&endTime);
	return (double)(endTime.QuadPart - startTime.QuadPart) / (double)perfFreq.QuadPart;
};


bool ComplexTut4::update()
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
void ComplexTut4::draw()
{
	Application::draw();
}
bool ComplexTut4::startup()
{
	if (Application::startup())
	{
		QueryPerformanceFrequency(&perfFreq);

		for (int i = 0; i < VECTOR_COUNT; ++i)
		{
			m_Vectors[i] = glm::vec4(i, i % 3, i % 4, i % 55556);
		}

		//CPU
		StartClock();
		for (int i = 0; i < VECTOR_COUNT; ++i)
		{
			m_Vectors[i] = glm::normalize(m_Vectors[i]);
		}
		printf("CPU Durration: %f \n", EndClock());


		//GPU
		cl_int result = clGetPlatformIDs(1, &m_platform, nullptr);
		if (result != CL_SUCCESS)
			printf("clGetPlatformIDs failed: %i\n", result);

		result = clGetDeviceIDs(m_platform, CL_DEVICE_TYPE_DEFAULT, 1, &m_device, nullptr);
		if (result != CL_SUCCESS)
			printf("clGetDeviceIDs failed: %i\n", result);

		cl_context_properties contextProperties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)m_platform, 0 };
		m_context = clCreateContext(contextProperties, 1, &m_device, nullptr, nullptr, &result);
		if (result != CL_SUCCESS)
			printf("clCreateContext failed: %i\n", result);

		m_queue = clCreateCommandQueue(m_context, m_device, CL_QUEUE_PROFILING_ENABLE, &result);
		if (result != CL_SUCCESS)
			printf("clCreateCommandQueue failed: %i\n", result);

		
		//file = fopen("kernal.cl", "rb");
		//if (file == nullptr)
		//	return false;
		//
		//fseek(file, 0, SEEK_END);
		//unsigned int kernalSize = ftell(file);
		//fseek(file, 0, SEEK_SET);
		//char* kernelSource = new char[kernalSize + 1];
		//memset(kernelSource, 0,  kernalSize+1);
		//fread(kernelSource, sizeof(char), kernalSize, file);
		//fclose(file);

		const char* kernelSource = "__kernel void normalizev4( \
									__global float4* vectors) { \
								   int i = get_global_id(0); \
								   vectors[i] = normalize(vectors[i]); \
	}";

		unsigned int kernelSize = strlen(kernelSource);


		m_program = clCreateProgramWithSource(m_context, 1, (const char**)&kernelSource, &kernelSize, &result);
		if (result != CL_SUCCESS)
			printf("clCreateProgramWithSource failed: %i\n", result);

		result = clBuildProgram(m_program, 1, &m_device, nullptr, nullptr, nullptr);
		if (result != CL_SUCCESS)
			printf("clBuildProgram failed: %i\n", result);

		m_kernel = clCreateKernel(m_program,"normalizev4", &result);
		if (result != CL_SUCCESS)
			printf("clCreateKernel failed: %i\n", result);

		m_buffer = clCreateBuffer(m_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(glm::vec4)*VECTOR_COUNT, m_Vectors, &result);
		if (result != CL_SUCCESS)
			printf("clCreateBuffer failed: %i\n", result);




		//Running the proccess - set the buffer as the first argument for our kernel
		result = clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &m_buffer);
		if (result != CL_SUCCESS)
			printf("clSetKernelArg failed: %i\n", result);

		//proccess the kernel and give it an event ID
		cl_event eventID = 0;
		size_t globalWorkSize[] = { VECTOR_COUNT };

		result = clEnqueueNDRangeKernel(m_queue, m_kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, &eventID);
		if (result != CL_SUCCESS)
			printf("clEnqueueNDRangeKernel failed: %i\n", result);

		//read back the processed data but wait for an event to complete
		result = clEnqueueReadBuffer(m_queue, m_buffer, CL_TRUE, 0, sizeof(glm::vec4)*VECTOR_COUNT, m_Vectors, 1, &eventID, nullptr);
		if (result != CL_SUCCESS)
			printf("clEnqueueReadBuffer failed: %i\n", result);

		clFlush(m_queue);
		clFinish(m_queue);

		//ClockStart
		cl_ulong clstarttime = 0;
		result = clGetEventProfilingInfo(eventID, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clstarttime, nullptr);
		if (result != CL_SUCCESS)
			printf("clGetEventProfilingInfo start failed: %i\n", result);


		//ClockEnd
		cl_ulong clendtime = 0;
		result = clGetEventProfilingInfo(eventID, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &clendtime, nullptr);
		if (result != CL_SUCCESS)
			printf("clGetEventProfilingInfo end failed: %i\n", result);

		double clTime = (clendtime - clstarttime)*1.0e-9;
		printf("GPU Duration: %f\n", clTime);


		return true;
	}
	else
	{
		return false;
	}

}

