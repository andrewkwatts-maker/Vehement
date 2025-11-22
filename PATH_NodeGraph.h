#pragma once
#include <vector>
#include <stack>
#include <queue>
#include <list>
#include <glm\glm.hpp>

using glm::vec3;

class PATH_Node;
class PATH_Edge;
using namespace std;
class PATH_NodeGraph
{
public:
	PATH_Node* AddNode();
	vector<PATH_Node*> GetNodeList();
	void RemoveNode(PATH_Node* NodePointer);
	void CalculateEdgeWeights(PATH_Node* NodePointer);
	void AddEdge(PATH_Edge Input);
	void RemoveEdge(PATH_Edge Input);
	//void SaveNodeGraph();
	//void LoadNodeGraph();
	void ResetForNewSearch(PATH_Node* Start, PATH_Node* Finish, bool SteppedSearch);
	bool DepthSearch();
	bool BreadthSearch();
	void DrawShortestPath();
	bool Dijkstras();
	bool A_star();
	float GetHeuristic(PATH_Node* Input);
	void BubbleSortH(std::vector<PATH_Node*> & data);
	float GetLength(PATH_Node* Input1, PATH_Node* Input2);
	PATH_Node* GetNearestNode(vec3 Input);
	PATH_Node* GetNearestNode(float X, float Y);
	//bool NodeCompare(Node* left,Node* right);
	void BuildRandomNetwork(int Nodes);
	void BuildSquareNetwork(int Length);
	//void CreateNetwork(vec3 Camera);
	bool NodeCompare(const PATH_Node& left, const PATH_Node& right);
	PATH_NodeGraph(void);
	~PATH_NodeGraph(void);

	//Assets
	bool SteppedSearch;
	bool Wait;
	PATH_Node* SearchFinish;
	PATH_Node* SearchStart;
private:
	vector<PATH_Node*> m_aNodeList;

	//SearchLists
	std::queue<PATH_Node*> oNodeQueue;
	std::stack<PATH_Node*> oNodeStack;
	std::vector<PATH_Node*> oNodeVector;
	std::vector<PATH_Node*> oNodeVectorA;

	//ConstuctionAssets
	PATH_Node* SelectedNode;
};

