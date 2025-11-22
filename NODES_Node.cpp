#include "NODES_Node.h"


//Node::Node()
//{
//	parent = nullptr;
//	localTransform = glm::mat4(1);
//}


Node::~Node()
{
	for (auto child : children)
		delete child;
}

NODES_Node::NODES_Node()
{
	NumChildren = 0;
};

NODES_Node::~NODES_Node()
{

};

void NODES_Node::AddChild(NODES_Node* Child)
{
	Children[NumChildren] = Child;
	++NumChildren;
};

void NODES_Node::Update()
{
	UpdateWorldTransform();

	for (int i = 0; i < NumChildren; ++i)
	{
		Children[i]->Update();
	}

	UpdateWorldBounds();

};

void NODES_Node::UpdateWorldTransform()
{
	if (Parent == nullptr)
		worldTransform = localTransform;
	else
		worldTransform = Parent->worldTransform*localTransform;
};
void NODES_Node::UpdateWorldBounds()
{
	worldBounds.centre = worldTransform*localBounds.centre;
	worldBounds.radius = localBounds.radius;

	for (int i = 0; i < NumChildren; ++i)
		worldBounds.fit(Children[i]->worldBounds);
};