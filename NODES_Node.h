#pragma once
#include <vector>
#include <glm\glm.hpp>
#include "NODES_BoundingSphere.h"
class Node
{
public:
	Node() :parent(nullptr), localTransform(1){};
	virtual	~Node();

	void Node::addChild(Node* child) {
		children.push_back(child);
	}

	void Node::update() {
		updateWorldTransform();

		for (auto child : children)
			child->update();

		updateWorldBounds();
	}

	void Node::updateWorldTransform() {
		if (parent == nullptr)
			worldTransform = localTransform;
		else
			worldTransform = parent->worldTransform*localTransform;
	}

	void Node::updateWorldBounds() {
		worldBounds.centre = worldTransform*localBounds.centre;
		worldBounds.radius = localBounds.radius;

		for (auto child : children)
			worldBounds.fit(child->worldBounds);
	}

protected:
	std::vector<Node*> children;

	NODES_BoundingSphere localBounds;
	NODES_BoundingSphere worldBounds;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	
	Node* parent;
};



class NODES_Node
{
public:
	NODES_Node();
	~NODES_Node();

	void AddChild(NODES_Node* Child);
	void Update();
	void UpdateWorldTransform();
	void UpdateWorldBounds();
protected:
	int NumChildren;

	NODES_BoundingSphere localBounds;
	NODES_BoundingSphere worldBounds;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	NODES_Node* Parent;
	NODES_Node* Children [];


};

//class Node
//{
//public:
//	Node() :parent(nullptr), localTransform(1){};
//
//	void Node::addChild(Node child) {
//		children.push_back(child);
//	}
//
//	void Node::update() {
//		updateWorldTransform();
//
//		for (auto child : children)
//			child.update();
//
//		updateWorldBounds();
//	}
//
//	void Node::updateWorldTransform() {
//		if (parent == nullptr)
//			worldTransform = localTransform;
//		else
//			worldTransform = parent->worldTransform*localTransform;
//	}
//
//	void Node::updateWorldBounds() {
//		worldBounds.centre = worldTransform*localBounds.centre;
//		worldBounds.radius = localBounds.radius;
//
//		for (auto child : children)
//			worldBounds.fit(child.worldBounds);
//	}
//
//protected:
//	std::vector<Node> children;
//
//	NODES_BoundingSphere localBounds;
//	NODES_BoundingSphere worldBounds;
//
//	glm::mat4 localTransform;
//	glm::mat4 worldTransform;
//
//
//	Node* parent;
//};