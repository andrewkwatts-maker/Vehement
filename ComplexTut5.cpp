#include "ComplexTut5.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"
#include "Camera.h"


ComplexTut5::ComplexTut5()
{
}


ComplexTut5::~ComplexTut5()
{
	cleanupOpenGLBuffers(m_fbx);

	glDeleteProgram(m_Program);
}

bool ComplexTut5::update()
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
void ComplexTut5::draw()
{
	glUseProgram(m_Program);


	//camera
	int loc = glGetUniformLocation(m_Program, "projectionView");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &appBasics->AppCamera->GetProjectionView()[0][0]);

	//model
	int mod = glGetUniformLocation(m_Program, "model");
	glUniformMatrix4fv(mod, 1, GL_FALSE, &mat4(1)[0][0]);

	//invTransposeModel
	int invmod = glGetUniformLocation(m_Program, "invTransposeModel");
	glUniformMatrix4fv(invmod, 1, GL_FALSE, &glm::inverse(mat4(1))[0][0]);

	for (unsigned int i = 0; i < m_sponza->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_sponza->getMeshByIndex(i);

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}


	//draw Graph
	for (int n = 0; n < m_graph.size(); ++n)
	{
		Gizmos::addAABBFilled(m_graph[n].position, vec3(0.07f), vec4(1, 0,0, 1), &mat4(1));

		if (m_graph[n].edgeTargets[0] != nullptr)
			Gizmos::addLine(m_graph[n].position, m_graph[n].edgeTargets[0]->position, vec4(1,0,0,1));
		if (m_graph[n].edgeTargets[1] != nullptr)
			Gizmos::addLine(m_graph[n].position, m_graph[n].edgeTargets[1]->position, vec4(1,0,0,1));
		if (m_graph[n].edgeTargets[2] != nullptr)
			Gizmos::addLine(m_graph[n].position, m_graph[n].edgeTargets[2]->position, vec4(1,0,0,1));
	}



	Application::draw();
}
bool ComplexTut5::startup()
{
	if (Application::startup())
	{

		vsSource = "#version 410 \n \
					layout( location = 0 ) in vec4 position; \n \
					layout( location = 1 ) in vec4 normal; \n \
					out	vec4 worldPosition; \n \
					out	vec4 worldNormal; \n \
					uniform	mat4 projectionView; \n\
					uniform	mat4 model; \n \
					uniform mat4 invTransposeModel; \n \
					void main() { \n \
					worldPosition = model * position; \n \
					worldNormal = invTransposeModel * normal; \n\
					gl_Position = projectionView * worldPosition; \n \
					}";

		fsSource =	"#version 410 \n \
					in vec4	worldPosition; \n \
					in vec4	worldNormal; \n \
					layout(location = 0) out vec4 fragColour; \n \
					void main() { \n \
					vec3 colour = vec3(1); \n \
					// grid every 1 -unit \n \
					if(	mod(worldPosition.x, 1.0) < 0.05f ||mod(worldPosition.y, 1.0) < 0.05f ||mod(worldPosition.z, 1.0) < 0.05f) \n \
						colour =vec3(0); \n \
					// fake light \n \
					float d = max(0,dot(normalize(vec3(1, 1, 1)),normalize(worldNormal.xyz))) * 0.75f; \n \
					fragColour.rgb = colour * 0.25f + colour * d; \n \
					fragColour.a = 1; \n \
					}";


		VertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(VertexShader, 1, (const char**)&vsSource, 0);
		glCompileShader(VertexShader);

		FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(FragmentShader, 1, (const char**)&fsSource, 0);
		glCompileShader(FragmentShader);

		m_Program = glCreateProgram();
		glAttachShader(m_Program, VertexShader);
		glAttachShader(m_Program, FragmentShader);
		glLinkProgram(m_Program);

		glDeleteShader(VertexShader);
		glDeleteShader(FragmentShader);


		//m_fbx = new FBXFile();
		//m_fbx->load("./FBX/Bunny.fbx",FBXFile::UNITS_METER,false,false,false);
		//m_fbx->load("./FBX/Bunny.fbx");
		//createOpenGLBuffers(m_fbx);

		m_sponza = new FBXFile();
		m_sponza->load("./data/SS/SS.fbx", FBXFile::UNITS_CENTIMETER);
		createOpenGLBuffers(m_sponza);

		m_navMesh = new FBXFile();
		m_navMesh->load("./data/SS/SSNM.fbx", FBXFile::UNITS_CENTIMETER);
		//createOpenGLBuffers(m_sponza);

		//load NavMesh

		int nodes = m_navMesh->getMeshByIndex(0)->m_vertices.size() / 3;

		for (int TriangleNum = 0; TriangleNum < nodes; TriangleNum++)
		{
			int indexa = m_navMesh->getMeshByIndex(0)->m_indices[TriangleNum * 3 + 0];
			int indexb = m_navMesh->getMeshByIndex(0)->m_indices[TriangleNum * 3 + 1];
			int indexc = m_navMesh->getMeshByIndex(0)->m_indices[TriangleNum * 3 + 2];

			m_graph.push_back(NavNode());
			m_graph[TriangleNum].edgeTargets[0] = nullptr;
			m_graph[TriangleNum].edgeTargets[1] = nullptr;
			m_graph[TriangleNum].edgeTargets[2] = nullptr;

			vec3 VertexA = vec3(m_navMesh->getMeshByIndex(0)->m_vertices[indexa].position);
			vec3 VertexB = vec3(m_navMesh->getMeshByIndex(0)->m_vertices[indexb].position);
			vec3 VertexC = vec3(m_navMesh->getMeshByIndex(0)->m_vertices[indexc].position);


			m_graph[TriangleNum].vertices[0] = VertexA;
			m_graph[TriangleNum].vertices[1] = VertexB;
			m_graph[TriangleNum].vertices[2] = VertexC;

			m_graph[TriangleNum].position = (VertexA + VertexB + VertexC) / 3;
		}

		for (int nodeA = 0; nodeA < nodes; ++nodeA)
		{
			for (int nodeB = 0; nodeB < nodes; ++nodeB)
			{
				if (nodeA != nodeB)
				{
					for (int i = 0; i < 3; ++i)
					{
						for (int j = 0; j < 3; ++j)
						{
							if (i != j)
							{
								if (m_graph[nodeA].vertices[0] == m_graph[nodeB].vertices[i])
								{
									if (m_graph[nodeA].vertices[1] == m_graph[nodeB].vertices[j])
									{
										m_graph[nodeA].edgeTargets[0] = &m_graph[nodeB];
										m_graph[nodeA].edgeCosts[0] = glm::length(m_graph[nodeA].position - m_graph[nodeB].position);
									}
								}
								if (m_graph[nodeA].vertices[1] == m_graph[nodeB].vertices[i])
								{
									if (m_graph[nodeA].vertices[2] == m_graph[nodeB].vertices[j])
									{
										m_graph[nodeA].edgeTargets[1] = &m_graph[nodeB];
										m_graph[nodeA].edgeCosts[1] = glm::length(m_graph[nodeA].position - m_graph[nodeB].position);
									}
								}
								if (m_graph[nodeA].vertices[2] == m_graph[nodeB].vertices[i])
								{
									if (m_graph[nodeA].vertices[0] == m_graph[nodeB].vertices[j])
									{
										m_graph[nodeA].edgeTargets[1] = &m_graph[nodeB];
										m_graph[nodeA].edgeCosts[1] = glm::length(m_graph[nodeA].position - m_graph[nodeB].position);
									}
								}
							}
						}
					}
				}
			}
		}

		//


		return true;
	}
	else
	{
		return false;
	}

}

void ComplexTut5::createOpenGLBuffers(FBXFile* fbx)
{
	for (unsigned int i = 0; i < fbx->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = fbx->getMeshByIndex(i);

		unsigned int* glData = new unsigned int[3];

		glGenVertexArrays(1, &glData[0]);
		glBindVertexArray(glData[0]);

		glGenBuffers(1, &glData[1]);
		glGenBuffers(1, &glData[2]);

		glBindBuffer(GL_ARRAY_BUFFER, glData[1]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glData[2]);

		glBufferData(GL_ARRAY_BUFFER, mesh->m_vertices.size()*sizeof(FBXVertex), mesh->m_vertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->m_indices.size()*sizeof(unsigned int), mesh->m_indices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0); //Position
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FBXVertex), 0);

		glEnableVertexAttribArray(1); //normal
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(FBXVertex), ((char*)0) + FBXVertex::NormalOffset);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		mesh->m_userData = glData;

	}
}

void ComplexTut5::cleanupOpenGLBuffers(FBXFile* fbx)
{
	for (unsigned int i = 0; i < fbx->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = fbx->getMeshByIndex(i);

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glDeleteVertexArrays(1, &glData[0]);
		glDeleteBuffers(1, &glData[1]);
		glDeleteBuffers(1, &glData[2]);

		delete[] glData;
	}
}