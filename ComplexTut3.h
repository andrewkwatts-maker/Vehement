#include "Application.h"
#include <Windows.h>
#include "NODES_Node.h"

class ComplexTut3 :public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	LARGE_INTEGER perfFreq;
	LARGE_INTEGER startTime, endTime;

	void StartClock();
	double EndClock();

	Node* RootNode;
	NODES_Node* RootNode2;
	void AddChildren(Node* Root, int childrenPerNode, int Depth);
	void AddChildren(NODES_Node* Root, int childrenPerNode, int Depth);

	ComplexTut3();
	~ComplexTut3();

};