#include "GL_Manager.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"


#include "stb-master\stb_image.h"
#include <fstream>
#include <iosfwd>
#include "aie\Gizmos.h"

using namespace std;

GL_Manager::GL_Manager()
{

}

void GL_Manager::SetNullFrameData(glm::vec2 ScreenSize)
{
	FrameTargetData Data;
	Data.FBO_GLNumber = 0;
	Data.HasDepthBuffer = true;
	Data.RenderTargetAttachments = 0;
	Data.Width = ScreenSize.x;
	Data.Height = ScreenSize.y;
	m_FrameTarget_FBOs.push_back(Data); // Add normal Backbuffer of screen
}


GL_Manager::~GL_Manager()
{
	for (unsigned int Model = 0; Model < m_FBX_Models.size(); ++Model)
	{
		cleanupFBXOpenGLBuffers(Model);
		delete m_FBX_Models[Model];
		//m_FBX_Models[Model] = nullptr;
	}

	for (unsigned int Program = 0; Program < m_Programs.size(); ++Program)
	{
		glDeleteProgram(m_Programs[Program]);
		delete Vertex_Shader_Source[Program];
		delete Fragment_Shader_Source[Program];
	}

}

int GL_Manager::AddTexture(char* TextureFile)
{
	int imageWidth = 0, imageHeight = 0, imageFormat = 0;
	unsigned char* data = stbi_load(TextureFile, &imageWidth, &imageHeight, &imageFormat, STBI_default);
	int ID = m_Textures.size();
	m_Textures.push_back(0);

	glGenTextures(1, &m_Textures[ID]);
	glBindTexture(GL_TEXTURE_2D, m_Textures[ID]);
	switch (imageFormat)
	{
	case STBI_rgb:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageWidth, imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		break;
	case STBI_rgb_alpha:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		break;
	case STBI_grey:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, imageWidth, imageHeight, 0, GL_RED, GL_UNSIGNED_BYTE, data);
		break;
	case STBI_grey_alpha:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, imageWidth, imageHeight, 0, GL_RG, GL_UNSIGNED_BYTE, data);
		break;
	default:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageWidth, imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		break;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	stbi_image_free(data);

	return ID;
}

void GL_Manager::SetTexture(int TextureID, int SlotNumber, char* UniformName)
{
	glActiveTexture(GL_TEXTURE0 + SlotNumber);
	glBindTexture(GL_TEXTURE_2D, m_Textures[TextureID]);
	glUniform1i(glGetUniformLocation(CurrentShader, UniformName), SlotNumber);
};

void GL_Manager::SetRenderTargetAsTexture(int RenderTargetID, int SlotNumber, char* UniformName)
{
	glActiveTexture(GL_TEXTURE0 + SlotNumber);
	glBindTexture(GL_TEXTURE_2D, m_RenderTargets[RenderTargetID].TargetID);
	glUniform1i(glGetUniformLocation(CurrentShader, UniformName), SlotNumber);
}

int GL_Manager::AddFBXModel(char* FBX_FILE)
{
	int ID = m_FBX_Models.size();
	FBXFile* FBX = new FBXFile();
	if (FBX->load(FBX_FILE,FBXFile::UNITS_METER))
	{
		FBX->initialiseOpenGLTextures();
		m_FBX_Models.push_back(FBX);
		createFBXOpenGLBuffers(ID);
		return ID;
	}

	return -1; // Didn't Load
}

void GL_Manager::createFBXOpenGLBuffers(int FBX_ID)
{
	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);

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
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FBXVertex), (void*)FBXVertex::PositionOffset);

		glEnableVertexAttribArray(1); //normal
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(FBXVertex), (void*)FBXVertex::NormalOffset); //NormalOffset

		glEnableVertexAttribArray(2); //TangentOffset
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_TRUE, sizeof(FBXVertex), (void*)FBXVertex::TangentOffset); //TangentOffset

		glEnableVertexAttribArray(1); //vTexCoord - Needs To Be Set back To 3
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FBXVertex), (void*)FBXVertex::TexCoord1Offset);

		glEnableVertexAttribArray(4); //WeightsOffset
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(FBXVertex), (void*)FBXVertex::WeightsOffset);

		glEnableVertexAttribArray(5); //IndiciesOffset
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(FBXVertex), (void*)FBXVertex::IndicesOffset);



		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		mesh->m_userData = glData;

	}
}

void GL_Manager::cleanupFBXOpenGLBuffers(int FBX_ID)
{
	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glDeleteVertexArrays(1, &glData[0]);
		glDeleteBuffers(1, &glData[1]);
		glDeleteBuffers(1, &glData[2]);

		delete[] glData;
	}
}


int  GL_Manager::AddCustomGeometry(vector<VertexComplex> Vertexs, vector<unsigned int> Indexs)
{
	unsigned int VBO;
	unsigned int VAO;
	unsigned int IBO;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex) * Vertexs.size(), &Vertexs[0], GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indexs.size(), &Indexs[0], GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 12);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 4);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 8);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	int Result = m_CustomGeometryVAOs.size();
	m_CustomGeometryVAOs.push_back(VAO);
	m_CustomGeometryIndexCount.push_back(Indexs.size());
	return Result;
}

UIVec4 GL_Manager::TemporaryCustomGeometry(vector<VertexComplex> Vertexs, vector<unsigned int> Indexs)
{
	unsigned int VBO;
	unsigned int VAO;
	unsigned int IBO;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex) * Vertexs.size(), &Vertexs[0], GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indexs.size(), &Indexs[0], GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 12);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 4);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 8);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return UIVec4(VAO, VBO, IBO, Indexs.size());

}

void GL_Manager::DeleteTempGeometry(UIVec4 TempID)
{
	glDeleteVertexArrays(1, &TempID.Data[0]);
	glDeleteBuffers(1, &TempID.Data[1]);
	glDeleteBuffers(1, &TempID.Data[2]);
}

int GL_Manager::AddCustomGeometry(vector<VertexBasicTextured> Vertexs, vector<unsigned int> Indexs)
{
	unsigned int VBO;
	unsigned int VAO;
	unsigned int IBO;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex) * Vertexs.size(), &Vertexs[0], GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indexs.size(), &Indexs[0], GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexBasicTextured), 0); //Position Data

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexBasicTextured), ((char*)0) + sizeof(float) * 4); //Texture UV Data

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	int Result = m_CustomGeometryVAOs.size();
	m_CustomGeometryVAOs.push_back(VAO);
	m_CustomGeometryIndexCount.push_back(Indexs.size());
	return Result;
}

int GL_Manager::AddscreenQuadGeometry(float Depth, glm::vec2 Min, glm::vec2 Max, glm::vec2 ScreenSize)
{
	vector<VertexBasicTextured> Points;
	vector<unsigned int> Indexs;
	glm::vec2 halfTexel = (1.0f / ScreenSize) *0.5f;
	Points.push_back({ Min.x, Min.y, Depth, 1, halfTexel.x, halfTexel.y });
	Points.push_back({ Max.x, Max.y, Depth, 1, 1 - halfTexel.x, 1 - halfTexel.y });
	Points.push_back({ Min.x, Max.y, Depth, 1, halfTexel.x, 1 - halfTexel.y });
	Points.push_back({ Max.x, Min.y, Depth, 1, 1 - halfTexel.x, halfTexel.y });

	Indexs.push_back(0);
	Indexs.push_back(1);
	Indexs.push_back(2);
	Indexs.push_back(0);
	Indexs.push_back(3);
	Indexs.push_back(1);

	return AddCustomGeometry(Points, Indexs);
}

int GL_Manager::AddFullscreenQuadGeometry(float Depth, glm::vec2 ScreenSize)
{
	vector<VertexBasicTextured> Points;
	vector<unsigned int> Indexs;
	glm::vec2 halfTexel = (1.0f / ScreenSize) *0.5f;
	Points.push_back({ -1, -1, Depth, 1, halfTexel.x, halfTexel.y });
	Points.push_back({ 1, 1, Depth, 1, 1- halfTexel.x,1 - halfTexel.y });
	Points.push_back({ -1, 1, Depth, 1, halfTexel.x,1- halfTexel.y });
	Points.push_back({ 1, -1, Depth, 1, 1- halfTexel.x, halfTexel.y });

	Indexs.push_back(0);
	Indexs.push_back(1);
	Indexs.push_back(2);
	Indexs.push_back(0);
	Indexs.push_back(3);
	Indexs.push_back(1);

	return AddCustomGeometry(Points, Indexs);

}



void GL_Manager::DrawCustomGeometry(int Geometry_ID, glm::vec3 Location)
{
	SetTransform(glm::translate(Location));
	glBindVertexArray(m_CustomGeometryVAOs[Geometry_ID]);
	glDrawElements(GL_TRIANGLES, m_CustomGeometryIndexCount[Geometry_ID], GL_UNSIGNED_INT, nullptr);
}

void GL_Manager::DrawTempCustomGeometry(UIVec4 Geometry_ID)
{
	glBindVertexArray(Geometry_ID.Data[0]);
	glDrawElements(GL_TRIANGLES, Geometry_ID.Data[3], GL_UNSIGNED_INT, nullptr);
}

void GL_Manager::UseFrameTarget(int FBO_ID)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FrameTarget_FBOs[FBO_ID].FBO_GLNumber);
	CurrentFrameTarget = FBO_ID;
}

void GL_Manager::ClearFrameTarget(glm::vec4 Colour)
{
	glClearColor(Colour.x,Colour.y,Colour.z,Colour.w);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GL_Manager::SetFrameTargetSize()
{
	glViewport(0, 0, m_FrameTarget_FBOs[CurrentFrameTarget].Width, m_FrameTarget_FBOs[CurrentFrameTarget].Height);
}

int GL_Manager::GenNewFrameTarget(int Width, int Height, bool HasDepthBuffer)
{
	int Ref = m_FrameTarget_FBOs.size();
	FrameTargetData Data;
	Data.FBO_GLNumber = 0;
	Data.HasDepthBuffer = HasDepthBuffer;
	Data.Width = Width;
	Data.Height = Height;
	Data.RenderTargetAttachments = 0;

	m_FrameTarget_FBOs.push_back(Data);
	glGenFramebuffers(1, &m_FrameTarget_FBOs[Ref].FBO_GLNumber);

	return Ref;
}

//void GL_Manager::UseRenderTarget(int FBO_ID,int RenderTarger_ID)
//{
//
//}

void GL_Manager::BeginNewDrawTo(int FBO_ID, vec4 BackgroundColour) //used for custom Render Targets
{
	UseFrameTarget(FBO_ID);
	SetFrameTargetSize();
	ClearFrameTarget(BackgroundColour);
	Gizmos::clear();
}
void GL_Manager::EndDrawCall(glm::mat4 ProjectionView)
{
	Gizmos::draw(ProjectionView);
}

int GL_Manager::GenNewRenderer() //Testing Purposes Only - DO NOT USE/ NOT IN USE
{
	GLuint FBO;
	GLuint DepthBuffer;
	GLuint RenderTarget;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glGenTextures(1, &RenderTarget);
	glBindTexture(GL_TEXTURE_2D, RenderTarget);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA8,GL_UNSIGNED_BYTE,0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenRenderbuffers(1, &DepthBuffer);


	bool fail = false;
	return 0;
}

int GL_Manager::GenNewRenderTarget(int FBO_ID, unsigned int ColourFormat)
{
	int Ref = m_RenderTargets.size();
	RenderTargetData Data;
	Data.FBO = FBO_ID;
	Data.TargetID = 0;
	Data.ColourFormat = ColourFormat;
	Data.FBODepthBuffer = 0;
	m_RenderTargets.push_back(Data);

	UseFrameTarget(FBO_ID);
	glGenTextures(1, &m_RenderTargets[Ref].TargetID);
	glBindTexture(GL_TEXTURE_2D, m_RenderTargets[Ref].TargetID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (ColourFormat == GL_DEPTH_COMPONENT)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, ColourFormat, m_FrameTarget_FBOs[FBO_ID].Width, m_FrameTarget_FBOs[FBO_ID].Height, 0, ColourFormat, GL_FLOAT, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,m_RenderTargets[Ref].TargetID, 0); //FBO MUST BE BOUND, Attaches texture to the bound FBO - I think colour attachment is possibly the number of attachments the FBO has.

		if (m_FrameTarget_FBOs[FBO_ID].HasDepthBuffer)
		{
			glGenRenderbuffers(1, &m_RenderTargets[Ref].FBODepthBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, m_RenderTargets[Ref].FBODepthBuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_FrameTarget_FBOs[FBO_ID].Width, m_FrameTarget_FBOs[FBO_ID].Height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_RenderTargets[Ref].FBODepthBuffer);

		}

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

	}
	if (ColourFormat == GL_RGBA8)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, ColourFormat, m_FrameTarget_FBOs[FBO_ID].Width, m_FrameTarget_FBOs[FBO_ID].Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);//unsure of order of width and height. example had them the same size.
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_FrameTarget_FBOs[FBO_ID].RenderTargetAttachments, m_RenderTargets[Ref].TargetID, 0); //FBO MUST BE BOUND, Attaches texture to the bound FBO - I think colour attachment is possibly the number of attachments the FBO has.
		
		if (m_FrameTarget_FBOs[FBO_ID].HasDepthBuffer)
		{
			glGenRenderbuffers(1, &m_RenderTargets[Ref].FBODepthBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, m_RenderTargets[Ref].FBODepthBuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_FrameTarget_FBOs[FBO_ID].Width, m_FrameTarget_FBOs[FBO_ID].Height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_RenderTargets[Ref].FBODepthBuffer);

		}

		++m_FrameTarget_FBOs[FBO_ID].RenderTargetAttachments;
		vector<GLenum> Attachments;
		for (int i = 0; i < m_FrameTarget_FBOs[FBO_ID].RenderTargetAttachments; ++i)
		{
			Attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
		}
		glDrawBuffers(m_FrameTarget_FBOs[FBO_ID].RenderTargetAttachments, &Attachments[0]);
	}
 //linear could be changed


	//glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_RenderTargets[Ref].TargetID, 0); //FBO MUST BE BOUND, Attaches texture to the bound FBO - I think colour attachment is possibly the number of attachments the FBO has.

	//Likely with have to ajust bellow to have an array of Colour attachments based on how many render targets and FBO has.

	//Before we unbind the FBO we must also tell it how many colour attachments we have assigned and which they are via a call to glDrawBuffers(), passing it the number of textures attached and an array of the enumerated GL_COLOR_ATTACHMENTN values.

	//GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	//glDrawBuffers(1, DrawBuffers);

	GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
		printf("FrameBuffer Error ! FBO: %i, RenderTarget %i \n", FBO_ID, m_RenderTargets[Ref].TargetID);

	return Ref;
}

//DrawFBX

void GL_Manager::DrawFBX(int FBX_ID, glm::mat4 Transform,float Time)
{
	SetTransform(Transform);

	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getAnimationCount(); ++i) //m_FBX_Models[FBX_ID]->getAnimationCount()
	{
		FBXAnimation* Anim = m_FBX_Models[FBX_ID]->getAnimationByIndex(i);
		FBXSkeleton* Skel = m_FBX_Models[FBX_ID]->getSkeletonByIndex(i);

		Skel->evaluate(Anim, Time,true,24); //Pass in delta time		
		for (unsigned int bone_index = 0; bone_index < Skel->m_boneCount; ++bone_index)
		{
			Skel->m_nodes[bone_index]->updateGlobalTransform();
		}
		Skel->updateBones();

		int bones_loc = glGetUniformLocation(CurrentShader, "bones");
		glUniformMatrix4fv(bones_loc, Skel->m_boneCount, GL_FALSE, (float*)Skel->m_bones);
		
		//i = m_FBX_Models[FBX_ID]->getAnimationCount();
	}

	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);
		mesh->m_globalTransform = Transform;

		for (int j = FBXMaterial::TextureTypes_Count; j < 32; ++j)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		for (int j = 0; j < FBXMaterial::TextureTypes_Count; ++j)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			if (mesh->m_material->textures[j] != nullptr)
			{
				glBindTexture(GL_TEXTURE_2D, mesh->m_material->textures[j]->handle);
				GLuint LocID;
				switch (j)
				{
				case FBXMaterial::DiffuseTexture:
					LocID = glGetUniformLocation(CurrentShader, "diffuse");
					glUniform1i(LocID, j);
				case FBXMaterial::NormalTexture:
					LocID = glGetUniformLocation(CurrentShader, "normal");
					glUniform1i(LocID, j);
				case FBXMaterial::SpecularTexture:
					LocID = glGetUniformLocation(CurrentShader, "specular");
					glUniform1i(LocID, j);
				}
				
			}
			else
				glBindTexture(GL_TEXTURE_2D, 0);
		}

		unsigned int* glData = (unsigned int*)mesh->m_userData;
		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}
}


void GL_Manager::DrawFBX(int FBX_ID, glm::vec3 Location)
{
	SetTransform(glm::translate(Location));

	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);

		//for (int j = FBXMaterial::TextureTypes_Count; j < 32; ++j)
		//{
		//	glActiveTexture(GL_TEXTURE0 + j);
		//	glBindTexture(GL_TEXTURE_2D, 0);
		//}

		for (int j = 0; j < FBXMaterial::TextureTypes_Count; ++j)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			if (mesh->m_material->textures[j] != nullptr)
				glBindTexture(GL_TEXTURE_2D, mesh->m_material->textures[j]->handle);
			else
				glBindTexture(GL_TEXTURE_2D, 0);
		}
		mesh->m_globalTransform = glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(Location, 1));

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}
}


void GL_Manager::DrawFBX(int FBX_ID, glm::mat4 Transform, bool UseFBXTextures)
{
	SetTransform(Transform);

	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{
		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);

		//for (int j = FBXMaterial::TextureTypes_Count; j < 32; ++j)
		//{
		//	glActiveTexture(GL_TEXTURE0 + j);
		//	glBindTexture(GL_TEXTURE_2D, 0);
		//}

		if (UseFBXTextures)
		{
			for (int j = 0; j < FBXMaterial::TextureTypes_Count; ++j)
			{
				glActiveTexture(GL_TEXTURE0 + j);
				if (mesh->m_material->textures[j] != nullptr)
					glBindTexture(GL_TEXTURE_2D, mesh->m_material->textures[j]->handle);
				else
					glBindTexture(GL_TEXTURE_2D, 0);
			}
		}


		//mesh->m_globalTransform = Transform;

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}
}


void GL_Manager::DrawFBX(int FBX_ID)
{
	SetTransform(glm::translate(glm::vec3(0)));

	for (unsigned int i = 0; i < m_FBX_Models[FBX_ID]->getMeshCount(); ++i)
	{

		FBXMeshNode* mesh = m_FBX_Models[FBX_ID]->getMeshByIndex(i);

		//for (int j = FBXMaterial::TextureTypes_Count; j < 32; ++j)
		//{
		//	glActiveTexture(GL_TEXTURE0 + j);
		//	glBindTexture(GL_TEXTURE_2D, 0);
		//}

		for (int j = 0; j < FBXMaterial::TextureTypes_Count; ++j)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			if (mesh->m_material->textures[j] != nullptr)
				glBindTexture(GL_TEXTURE_2D, mesh->m_material->textures[j]->handle);
			else
				glBindTexture(GL_TEXTURE_2D, 0);
		}

		unsigned int* glData = (unsigned int*)mesh->m_userData;

		glBindVertexArray(glData[0]);
		glDrawElements(GL_TRIANGLES, (unsigned int)mesh->m_indices.size(), GL_UNSIGNED_INT, 0);
	}
}

//UNIFORMS
//====================================================================================

//Float Uniforms
template <>
void GL_Manager::PassInUniform(char* Name, float Value)
{
	unsigned int UniformID = glGetUniformLocation(CurrentShader, Name);
	glUniform1f(UniformID, Value);
};

//Vec3 Uniforms
template <>
void GL_Manager::PassInUniform(char* Name, glm::vec3 Value)
{
	unsigned int UniformID = glGetUniformLocation(CurrentShader, Name);
	glUniform3fv(UniformID, 1, glm::value_ptr(Value));
};

//Vec4 Uniforms
template <>
void GL_Manager::PassInUniform(char* Name, glm::vec4 Value)
{
	unsigned int UniformID = glGetUniformLocation(CurrentShader, Name);
	glUniform4fv(UniformID, 1, glm::value_ptr(Value));
};

//Mat4 Uniforms
template <>
void GL_Manager::PassInUniform(char* Name, glm::mat4 Value)
{
	unsigned int UniformID = glGetUniformLocation(CurrentShader, Name);
	glUniformMatrix4fv(UniformID, 1, GL_FALSE, &Value[0][0]);
};

//sampler2D Uniforms
template <>
void GL_Manager::PassInUniform(char* Name, GLint TextureNumber)
{
	unsigned int UniformID = glGetUniformLocation(CurrentShader, Name);
	glUniform1i(UniformID,TextureNumber);
};

void GL_Manager::SetTransform(glm::mat4 Transform)
{
	PassInUniform("Transform", Transform);
}


std::string GL_Manager::readFile(const char* FileName)
{
	string ContentsOfFile;
	ifstream File_Streaming(FileName, ios::in);

	if (!File_Streaming.is_open())
	{
		printf(FileName, " - Could not be read Successfully \n");
		return "";
	}

	string NextLine = "";
	while (!File_Streaming.eof())
	{
		getline(File_Streaming, NextLine);
		ContentsOfFile.append(NextLine + "\n ");
	}

	File_Streaming.close();
	return ContentsOfFile;

}

int GL_Manager::AddShaders(char* VS_FILE, char* FS_FILE)
{
	int ID = Vertex_Shader_Source.size();

	string VertexShader_String = readFile(VS_FILE);
	string FragmentShader_String = readFile(FS_FILE);

	Vertex_Shader_Source.push_back(VertexShader_String.c_str());
	Fragment_Shader_Source.push_back(FragmentShader_String.c_str());
	Geometry_Shader_Source.push_back("");

	VertexFileLocations.push_back(VS_FILE);
	FragmentFileLocations.push_back(FS_FILE);
	GeometryFileLocations.push_back((char*)(0));

	VertexShaders.push_back(glCreateShader(GL_VERTEX_SHADER));
	glShaderSource(VertexShaders[ID], 1, (const char**)&Vertex_Shader_Source[ID], 0);
	glCompileShader(VertexShaders[ID]);

	FragmentShaders.push_back(glCreateShader(GL_FRAGMENT_SHADER));
	glShaderSource(FragmentShaders[ID], 1, (const char**)&Fragment_Shader_Source[ID], 0);
	glCompileShader(FragmentShaders[ID]);

	GeometryShaders.push_back(0);

	m_Programs.push_back(glCreateProgram());
	glAttachShader(m_Programs[ID], VertexShaders[ID]);
	glAttachShader(m_Programs[ID], FragmentShaders[ID]);
	glLinkProgram(m_Programs[ID]);

	glDeleteShader(VertexShaders[ID]); //Nolonger Needed
	glDeleteShader(FragmentShaders[ID]); //Nolonger Needed


	return ID;
}

int GL_Manager::AddShaders(char* VS_FILE, char* FS_FILE, char* GS_FILE)
{
	int ID = Vertex_Shader_Source.size();

	string VertexShader_String = readFile(VS_FILE);
	string FragmentShader_String = readFile(FS_FILE);
	string GeometryShader_String = readFile(GS_FILE);

	Vertex_Shader_Source.push_back(VertexShader_String.c_str());
	Fragment_Shader_Source.push_back(FragmentShader_String.c_str());
	Geometry_Shader_Source.push_back(GeometryShader_String.c_str());

	VertexFileLocations.push_back(VS_FILE);
	FragmentFileLocations.push_back(FS_FILE);
	GeometryFileLocations.push_back(GS_FILE);

	VertexShaders.push_back(glCreateShader(GL_VERTEX_SHADER));
	glShaderSource(VertexShaders[ID], 1, (const char**)&Vertex_Shader_Source[ID], 0);
	glCompileShader(VertexShaders[ID]);

	FragmentShaders.push_back(glCreateShader(GL_FRAGMENT_SHADER));
	glShaderSource(FragmentShaders[ID], 1, (const char**)&Fragment_Shader_Source[ID], 0);
	glCompileShader(FragmentShaders[ID]);

	GeometryShaders.push_back(glCreateShader(GL_GEOMETRY_SHADER));
	glShaderSource(GeometryShaders[ID], 1, (const char**)&Geometry_Shader_Source[ID], 0);
	glCompileShader(GeometryShaders[ID]);

	m_Programs.push_back(glCreateProgram());
	glAttachShader(m_Programs[ID], VertexShaders[ID]);
	glAttachShader(m_Programs[ID], FragmentShaders[ID]);
	glAttachShader(m_Programs[ID], GeometryShaders[ID]);
	glLinkProgram(m_Programs[ID]);

	glDeleteShader(VertexShaders[ID]); //Nolonger Needed
	glDeleteShader(FragmentShaders[ID]); //Nolonger Needed
	glDeleteShader(GeometryShaders[ID]); //Nolonger Needed


	return ID;
}

int GL_Manager::AddUpdateShader(char* VS_FILE, const char* Varyings[], int numberVaryings)
{
	int ID = UpdateShaders.size();
	string VertexShader_String = readFile(VS_FILE);
	UpdateShaders_Source.push_back(VertexShader_String.c_str());
	unsigned int shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shader, 1, (const char**)&UpdateShaders_Source[ID], 0);
	glCompileShader(shader);

	UpdateShaders.push_back(glCreateProgram());
	glAttachShader(UpdateShaders[ID],shader);

	glTransformFeedbackVaryings(UpdateShaders[ID], numberVaryings, Varyings, GL_INTERLEAVED_ATTRIBS);

	glLinkProgram(UpdateShaders[ID]);
	glDeleteShader(shader);

	return ID;
}


void GL_Manager::ReloadShader(int ShaderID)
{
	if (ShaderID < VertexFileLocations.size())
	{
		string VertexShader_String = readFile(VertexFileLocations[ShaderID]);
		string FragmentShader_String = readFile(VertexFileLocations[ShaderID]);

		Vertex_Shader_Source[ShaderID] = (VertexShader_String.c_str());
		Fragment_Shader_Source[ShaderID] = (FragmentShader_String.c_str());

		VertexShaders[ShaderID] = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(VertexShaders[ShaderID], 1, (const char**)&Vertex_Shader_Source[ShaderID], 0);
		glCompileShader(VertexShaders[ShaderID]);

		FragmentShaders[ShaderID] = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(FragmentShaders[ShaderID], 1, (const char**)&Fragment_Shader_Source[ShaderID], 0);
		glCompileShader(FragmentShaders[ShaderID]);

		glDeleteProgram(m_Programs[ShaderID]);
		m_Programs[ShaderID] = glCreateProgram();
		glAttachShader(m_Programs[ShaderID], VertexShaders[ShaderID]);
		glAttachShader(m_Programs[ShaderID], FragmentShaders[ShaderID]);
		glLinkProgram(m_Programs[ShaderID]);

		glDeleteShader(VertexShaders[ShaderID]); //Nolonger Needed
		glDeleteShader(FragmentShaders[ShaderID]); //Nolonger Needed

	}
}

void GL_Manager::UseUpdateShader(int ShaderID)
{
	glUseProgram(UpdateShaders[ShaderID]);
	CurrentShader = UpdateShaders[ShaderID];
}

void GL_Manager::UseShader(int ShaderID)
{
	glUseProgram(m_Programs[ShaderID]);
	CurrentShader = m_Programs[ShaderID];
	//glDeleteProgram(m_Program);
	//m_Program = glCreateProgram();
	//glAttachShader(m_Program, VertexShaders[ShaderID]);
	//glAttachShader(m_Program, FragmentShaders[ShaderID]);


	//glLinkProgram(m_Programs[ShaderID]);
}
