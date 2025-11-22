#pragma once
#include <vector>
#include <aie\FBXFile.h>
#include "Vertex.h"

#include "UIVec4.h"
using namespace std;

class FBXFile;

struct RenderTargetData
{
	unsigned int TargetID;
	unsigned int FBO;
	int ColourFormat;
	unsigned int FBODepthBuffer;
};

struct FrameTargetData
{
	unsigned int FBO_GLNumber;
	unsigned int Width;
	unsigned int Height;
	bool HasDepthBuffer;
	unsigned int RenderTargetAttachments;

};

struct Renderer
{
	unsigned int FBO_GLNumber;
	unsigned int RenderTarget_GLNumber;
	int ColourFormat;
	unsigned int DepthBuffer_GLNumber;
	unsigned int Width;
	unsigned int Height;
	bool HasDepthBuffer;

};

class GL_Manager
{
public:
	int AddFBXModel(char* FBX_FILE);
	int AddCustomGeometry(vector<VertexComplex> Vertexs, vector<unsigned int> Indexs);
	static UIVec4 TemporaryCustomGeometry(vector<VertexComplex> Vertexs, vector<unsigned int> Indexs); // returns a GL Number and doesnt store it in the GL Manager
	static void DeleteTempGeometry(UIVec4 TempID);
	int AddCustomGeometry(vector<VertexBasicTextured> Vertexs, vector<unsigned int> Indexs);
	int AddFullscreenQuadGeometry(float Depth,glm::vec2 ScreenSize);
	int AddFullscreenQuadGeometryCam(float Depth, glm::vec2 ScreenSize);
	int AddscreenQuadGeometry(float Depth, glm::vec2 Min,glm::vec2 Max, glm::vec2 ScreenSize);

	void createFBXOpenGLBuffers(int FBX_ID);
	void cleanupFBXOpenGLBuffers(int FBX_ID);
	int AddShaders(char* VS_FILE, char* FS_FILE);
	int AddShaders(char* VS_FILE, char* FS_FILE,char* GS_FILE);
	int AddUpdateShader(char* VS_FILE, const char* Varyings[],int numberVaryings);

	int AddShadersViaText(string VS, string FS);
	void UpdateShaderViaText(string VS, string FS,int Program);

	int AddTexture(char* TextureFile);
	//int AddTexture(unsigned char* Data);
	void ReloadShader(int ShaderID);
	void UseShader(int ShaderID);
	void UseUpdateShader(int ShaderID);
	void SetTransform(glm::mat4 Trans);
	void SetTexture(int TextureID,int SlotNumber, char* UniformName);
	void SetRenderTargetAsTexture(int RenderTargetID, int SlotNumber, char* UniformName);

	void BeginNewDrawTo(int FBO_ID, vec4 BackgroundColour); //used for custom Render Targets
	void EndDrawCall(glm::mat4 ProjectionView); //used for custom Render Targets

	int GenNewRenderTarget(int FBO_ID,unsigned int ColourFormat);
	void UseFrameTarget(int FBO_ID);
	void ClearFrameTarget(glm::vec4 Colour);
	void SetFrameTargetSize();
	int GenNewFrameTarget(int Width,int Height,bool HasDepthBuffer);

	int GenNewRenderer(); //RenderFrameAndTarget;

	static std::string readFile(const char* FileName);

	void DrawFBX(int FBX_ID, glm::mat4 Transform,float Time);
	void DrawFBX(int FBX_ID, glm::vec3 Location);
	void DrawFBX(int FBX_ID, glm::mat4 Transform,bool UseFBXTextures);
	void DrawFBX(int FBX_ID);

	void DrawCustomGeometry(int Geometry_ID, glm::vec3 Location);
	static void DrawTempCustomGeometry(UIVec4 Geometry_ID);

	template <typename T>
	void PassInUniform(char* Name, T Value);

	vector<unsigned int> m_Programs;
	unsigned int CurrentShader;

	vector<unsigned int> m_Textures;
	vector<FBXFile*> m_FBX_Models;

	vector<unsigned int> m_CustomGeometryVAOs;
	vector<unsigned int> m_CustomGeometryIndexCount;

	vector<FrameTargetData> m_FrameTarget_FBOs; //FBO are frame buffers, slot zero is for the standard back buffer of the screen.
	unsigned int CurrentFrameTarget;
	vector<RenderTargetData> m_RenderTargets;


	vector<Renderer> m_RenderFrames;


	vector<const char*> Vertex_Shader_Source;
	vector<const char*> Fragment_Shader_Source;
	vector<const char*> Geometry_Shader_Source;
	vector<const char*> UpdateShaders_Source;
	vector<char*> VertexFileLocations;
	vector<char*> FragmentFileLocations;
	vector<char*> GeometryFileLocations;
	vector<unsigned int> VertexShaders;
	vector<unsigned int> FragmentShaders;
	vector<unsigned int> GeometryShaders;

	vector<unsigned int> UpdateShaders; //used in particle systems to update data on the GPU


	void SetNullFrameData(glm::vec2 ScreenSize); //needed if going to use FrameBuffers;
	GL_Manager();
	~GL_Manager();
};

