#pragma once
#include <vector>
#include <glm\glm.hpp>

using glm::vec3;

using namespace std;
class PATH_Edge;
class PATH_Node :public vec3
{
public:
	float GetGScore();
	void SetGScore(float input);
	float GetFScore();
	void SetFScore(float input);
	PATH_Node* GetLastNode();
	void SetLastNode(PATH_Node* Input);
	void SetNodeNumber(int iNodeNumber);
	int GetNodeNumber();
	void AddEdge(PATH_Edge Addition);
	void RemoveEdge(PATH_Node* NodePointer);
	vector<PATH_Edge> GetEdges();
	void SetVisited(bool Value);
	bool GetVisited();

	bool operator < (PATH_Node* str);

	vector<PATH_Edge> m_aEdges;

	PATH_Node(void);
	~PATH_Node(void);
private:
	int m_iNodeNumber;
	bool m_bVisited;
	float m_fGscore;
	float m_fFscore;
	PATH_Node* m_nLastNode;
	
};

