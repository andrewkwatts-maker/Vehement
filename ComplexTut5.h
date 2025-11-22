#pragma once
#include "Application.h"
#include "aie\FBXFile.h"
#include "NavNode.h"
#include <vector>

class ComplexTut5 :public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	void createOpenGLBuffers(FBXFile* fbx);
	void cleanupOpenGLBuffers(FBXFile* fbx);

	FBXFile* m_fbx;

	FBXFile* m_sponza;
	FBXFile* m_navMesh;

	std::vector<NavNode> m_graph;

	unsigned int m_Program;

	const char* vsSource;
	const char* fsSource;

	unsigned int VertexShader;
	unsigned int FragmentShader;

	ComplexTut5();
	~ComplexTut5();
};
