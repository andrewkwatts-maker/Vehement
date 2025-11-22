#include "ComplexTut2.h"
#include <thread>
#include <stdio.h>
#include <vector>
#include <mutex>

using glm::vec4;

ComplexTut2::ComplexTut2()
{
	//printText();

	std::thread myThread(printText,0);
	myThread.join();

	std::vector<std::thread> Threads;

	for (int i = 1; i <= 5; i++)
	{
		Threads.push_back(std::thread(printText,i));
	}
	
	for (auto& threadID : Threads)
	{
		threadID.join();
	}

	std::thread LambdaThread(
		// defining a lambda with no parameters
		[](){
		printf("Lambda Thread\n");
		}
	);

	LambdaThread.join();

	std::mutex myMutex;
	std::thread MutexLambdaThread(
		// defining a lambda with no parameters
		[&myMutex](){
		std::lock_guard<std::mutex> guard(myMutex);
		printf("MutexLambda Thread\n");
		}
	);

	MutexLambdaThread.join();
	int i = -33;
	std::thread MutexLambdaThread2(
		// defining a lambda with no parameters
		[&myMutex](int i){
		std::lock_guard<std::mutex> guard(myMutex);
		printf("MutexLambda Thread%i\n",i);
	},i
	);

	MutexLambdaThread2.join();

	//ConcurrentThreads


	//Linnier
	std::vector<std::thread> ConThreads1;
	vec4 myVectors[50] = {};

	for (int i = 0; i < 50; i++)
	{
		ConThreads1.push_back(std::thread(
			[&](int i){
			myVectors[i] = glm::normalize(myVectors[i]);
			},i
			));
	}

	for (auto& threadID : ConThreads1)
	{
		threadID.join();
	}

	//Concurrent

	std::vector<std::thread> ConThreads2;
	int const NumberVectors = 50000;
	int const Chunks = 10;

	int ChunkLength = NumberVectors / Chunks;

	vec4 myVectors2[NumberVectors] = {};

	for (int i = 0; i < Chunks; i++)
	{
		ConThreads2.push_back(std::thread(
			[&](int low,int high){
				for (int j = low; j < high; j++)
				{
					myVectors2[i] = glm::normalize(myVectors2[i]);
				}
			}, i*ChunkLength, (i+1)*ChunkLength
		));
	}

	for (auto& threadID : ConThreads2)
	{
		threadID.join();
	}

}

void ComplexTut2::printText(int i)
{
	static std::mutex myMutex;

	myMutex.lock();

	printf("Hello Thread%i\n",i);
	printf("I'm here...\n");
	printf("...not there.\n");

	myMutex.unlock();
}

bool ComplexTut2::update()
{
	//printText();
	//std::thread myThread();
	//myThread.join();
	//
	//std::vector<std::thread> Threads;

	//for (int i = 0; i < 50; i++)
	//{
	//	Threads.push_back(std::thread(&ComplexTut2::print));
	//}
	//
	//for (auto& threadID : Threads)
	//{
	//	threadID.join();
	//}

	return Application::update();
}

void ComplexTut2::draw()
{
	Application::draw();
}


ComplexTut2::~ComplexTut2()
{
}
