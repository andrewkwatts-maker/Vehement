#include "ComplexTut3.h"
#include <Windows.h>
#include <iosfwd>


bool ComplexTut3::update()
{
	if (Application::update())
	{
		StartClock();
		RootNode->update();
		double Result = EndClock();
		printf("%f \n",Result);
		return true;
	}
	else
	{
		return false;
	}
}
void ComplexTut3::draw()
{
	Application::draw();
}
bool ComplexTut3::startup()
{
	if (Application::startup())
	{
		QueryPerformanceFrequency(&perfFreq);


		RootNode = new Node();
		AddChildren(RootNode, 4, 6);

		//RootNode2 = new NODES_Node();
		//AddChildren(RootNode2, 4, 6);


		return true;
	}
	else
	{
		return false;
	}

}

void ComplexTut3::StartClock()
{
	QueryPerformanceCounter(&startTime);
};
double ComplexTut3::EndClock()
{
	QueryPerformanceCounter(&endTime);
	return (double)(endTime.QuadPart - startTime.QuadPart) / (double)perfFreq.QuadPart;
};

void ComplexTut3::AddChildren(Node* Root, int childrenPerNode, int Depth)
{
	if (Depth > 0)
	{
		for (int i = 0; i < childrenPerNode; i++)
		{
			Node* Child = new Node();
			AddChildren(Child, childrenPerNode, Depth - 1);
			Root->addChild(Child);
		}
	}
}

void ComplexTut3::AddChildren(NODES_Node* Root, int childrenPerNode, int Depth)
{
	if (Depth > 0)
	{
		for (int i = 0; i < childrenPerNode; i++)
		{
			NODES_Node* Child = new NODES_Node();
			AddChildren(Child, childrenPerNode, Depth - 1);
			Root->AddChild(Child);
		}
	}
}

ComplexTut3::ComplexTut3()
{
}


ComplexTut3::~ComplexTut3()
{
	delete RootNode;
}
