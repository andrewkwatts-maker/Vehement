#include "GraphicsTut4.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"
#include "Camera.h"
#include "GL_Manager.h"
GraphicsTut4::GraphicsTut4()
{
}


GraphicsTut4::~GraphicsTut4()
{
	cleanupOpenGLBuffers(m_fbx);

	glDeleteProgram(m_Program);
}

bool GraphicsTut4::update()
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
void GraphicsTut4::draw()
{
	glUseProgram(m_Program);

	//Uniforms
	unsigned int LightLocUniform = glGetUniformLocation(m_Program, "LightDir");
	glUniform3fv(LightLocUniform, sizeof(vec3), glm::value_ptr(vec3(appBasics->AppCamera->GetWorldTransform()[3])));

	//Uniforms
	unsigned int LightColourUniform = glGetUniformLocation(m_Program, "LightColour");
	glUniform3fv(LightColourUniform, sizeof(vec3), glm::value_ptr(vec3(1, 1, 0.5f)));

	//Uniforms
	unsigned int CameraUniform = glGetUniformLocation(m_Program, "CameraPos");
	glUniform3fv(CameraUniform, sizeof(vec3), glm::value_ptr(vec3(appBasics->AppCamera->GetWorldTransform()[3])));

	//Uniforms
	unsigned int SpecUniform = glGetUniformLocation(m_Program, "SpecPower");
	glUniform1f(SpecUniform, 1.0f);

	//Uniforms
	unsigned int BrightnessUniform = glGetUniformLocation(m_Program, "Brightness");
	glUniform1f(BrightnessUniform, 5.0f);

	//camera
	int loc = glGetUniformLocation(m_Program, "ProjectionView");
	glUniformMatrix4fv(loc, 1, GL_FALSE,&appBasics->AppCamera->GetProjectionView()[0][0]);

	for (unsigned int i = 0; i < m_fbx->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_fbx->getMeshByIndex(i);

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}





	Application::draw();
}
bool GraphicsTut4::startup()
{
	if (Application::startup())
	{
		vsSource =	"#version 410\n	\
			layout(location = 0) in vec4 Position;\n\
			layout(location = 1) in vec4 Normal;\n\
			out vec4 vNormal;\n\
			out vec4 vPosition;\n\
			uniform mat4 ProjectionView;\n\
			void main() { \n vNormal = Normal;	\n\
			vPosition = Position; \n \
			gl_Position = ProjectionView*Position;	}";

		//string this1 = OGL_Manager->readFile("./Shaders/VS_PointLight.vert");
		//vsSource = this1.c_str();

		//int ID = OGL_Manager->AddShaders("./Shaders/VS_PointLight.vert", "./Shaders/FS_PointLight.frag");
		//m_Program = OGL_Manager->m_Programs[ID];




		//Direcional
		//fsSource = "#version 410\n\     
		//		   	in vec4	vNormal;\n\
		//			in vec4 vPosition; \n \
		//			out vec4 FragColor;\n\
		//			uniform vec3 LightDir; \n \
		//			uniform vec3 LightColour; \n \
		//			uniform vec3 CameraPos; \n \
		//			uniform float SpecPower; \n \
		//			void main() { \n \
		//			float d = max(0,dot( normalize(vNormal.xyz),LightDir)); \n\
		//			vec3 E = normalize(CameraPos - vPosition.xyz); \n \
		//			vec3 R = reflect(-LightDir,vNormal.xyz); \n \
		//			float s = max(0,dot(E,R)); \n \
		//			s = pow(s,SpecPower); \n \
		//			FragColor = vec4(d*LightColour +s*LightColour, 1);}";

		//fsSource = (OGL_Manager->readFile("./Shaders/FS_PointLight.frag")).c_str();
		//Point

		//string This2 = OGL_Manager->readFile("./Shaders/FS_PointLight.frag");
		//fsSource = This2.c_str();

		fsSource = "#version 410\n\
		   	in vec4	vNormal;\n\
			in vec4 vPosition; \n \
			out vec4 FragColor;\n\
			uniform vec3 LightDir; \n \
			uniform vec3 LightColour; \n \
			uniform vec3 CameraPos; \n \
			uniform float SpecPower; \n \
			uniform float Brightness; \n \
			void main() { \n \
			float d = max(0,dot( normalize(vNormal.xyz),normalize(LightDir-vPosition.xyz))); \n\
			float Intensity =  Brightness/length(LightDir-vPosition.xyz); \n \
			vec3 E = normalize(CameraPos - vPosition.xyz); \n \
			vec3 R = reflect(-LightDir,vNormal.xyz); \n \
			float s = max(0,dot(E,R)); \n \
			s = pow(s*Intensity,SpecPower); \n \
			FragColor = vec4(Intensity*(d*LightColour +s*LightColour), 1);}";

		//fsSource = "#version 410\n\
		//   	in vec4	vNormal;\
		//	out vec4 FragColor;\
		//	uniform vec3 LightDirection;\
		//	unifrom vec3 LightColour;\
		//	void main() { \
		//	float d = max(0,dot( normalize(vNormal.xyz),LightDirection)); \
		//	FragColor = vec4(d*LightColour.x, d*LightColour.y, d*LightColour.z, 1);}";

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


		m_fbx = new FBXFile();

		//m_fbx->load("./FBX/Bunny.fbx",FBXFile::UNITS_METER,false,false,false);
		m_fbx->load("./FBX/Bunny.fbx");
		createOpenGLBuffers(m_fbx);

		return true;
	}
	else
	{
		return false;
	}

}

void GraphicsTut4::createOpenGLBuffers(FBXFile* fbx)
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

void GraphicsTut4::cleanupOpenGLBuffers(FBXFile* fbx)
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