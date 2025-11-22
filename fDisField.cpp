#include "fDisField.h"
#include <vector>
#include "GL_Manager.h"
#include "Camera.h"
#include "AntTweakBar.h"

fDisField::fDisField()
{

}

fDisField::~fDisField()
{
}

bool fDisField::update()
{
	string FS_NEW = FS_S1;
	FS_NEW.insert(fFieldLoc, "return length(Loc)-3; \n");

	OGL_Manager->UpdateShaderViaText(VS_S1, FS_NEW, fDistanceShader);
	return Application::update();
};

void fDisField::draw()
{
	OGL_Manager->UseShader(fDistanceShader);
	OGL_Manager->PassInUniform("CamLoc", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("CamPointAt", appBasics->AppCamera->GetPos()+appBasics->AppCamera->GetDirVector());
	OGL_Manager->PassInUniform("FOV", (FOV/180.0f)*3.1415926f);
	OGL_Manager->PassInUniform("Tilt", (Tilt / 180.0f)*3.1415926f);
	OGL_Manager->PassInUniform("SphericalLensingRatio", (Lensing / 100.0f));
	OGL_Manager->PassInUniform("AspectRatio", appBasics->ScreenSize.x / appBasics->ScreenSize.y);
	OGL_Manager->PassInUniform("MaxSteps", MaxSteps);
	OGL_Manager->DrawCustomGeometry(CameraLensePlane, vec3(0, 0, 0));
	TwDraw();
	Application::draw();
};

void AddLine2Str(string* Base, string Input)
{
	Base->append(Input.append("\n"));
}

bool fDisField::startup()
{
	if (Application::startup())
	{
		//Menu
		Menu = TwNewBar("Parralex - Andrew W");
		Tilt = 0;
		FOV = 60;
		Lensing = 0;
		MaxSteps = 100;
		TwAddVarRW(Menu, "TiltOfCamera", TW_TYPE_FLOAT, &Tilt, "");
		TwAddVarRW(Menu, "FeildOfView", TW_TYPE_FLOAT, &FOV, "");
		TwAddVarRW(Menu, "FishEye", TW_TYPE_FLOAT, &Lensing, "");
		TwAddVarRW(Menu, "MaxSteps", TW_TYPE_INT16, &MaxSteps, "");

		string VS_Source;
		string FS_Source;
		string F_FieldCode;

		//VertexShader
		//==========================================================================================================================================
		//==========================================================================================================================================
		AddLine2Str(&VS_Source, "		#version 410																								  ");
		AddLine2Str(&VS_Source, "		layout(location = 0) in vec4 Position;																		  ");
		AddLine2Str(&VS_Source, "		layout(location = 1) in vec2 TexCoord;																		  ");
		AddLine2Str(&VS_Source, "		out vec2 vTexCoord;																							  ");
		AddLine2Str(&VS_Source, "																													  ");
		AddLine2Str(&VS_Source, "																													  ");
		AddLine2Str(&VS_Source, "		void main()																									  ");
		AddLine2Str(&VS_Source, "		{																											  ");
		AddLine2Str(&VS_Source, "			vTexCoord = TexCoord;																					  ");
		AddLine2Str(&VS_Source, "			gl_Position = Position;																					  ");
		AddLine2Str(&VS_Source, "		};																											  ");

		//Fragment Shader
		//==========================================================================================================================================
		//==========================================================================================================================================
		AddLine2Str(&FS_Source, "		#version 410																								 ");
		AddLine2Str(&FS_Source, "		in vec2	vTexCoord;																							 ");
		AddLine2Str(&FS_Source, "		out vec4 FragColor;																							 ");
		AddLine2Str(&FS_Source, "																													 ");
		AddLine2Str(&FS_Source, "		uniform vec3 CamLoc; //position of observation																 ");
		AddLine2Str(&FS_Source, "		uniform vec3 CamPointAt; //position of focus																 ");
		AddLine2Str(&FS_Source, "		uniform vec3 LenseScreenDimension							"); //width,Height,depth determining FOV
		AddLine2Str(&FS_Source, "																	");
		AddLine2Str(&FS_Source, "		vec3 RayStartLoc()											");
		AddLine2Str(&FS_Source, "		{															");
		AddLine2Str(&FS_Source, "			vec2 ScreenDisp = vTexCoord*ScreenDimensions.xy*0.5f;	");
		AddLine2Str(&FS_Source, "			vec3 Forward = normalize(CamPointAt-CamLoc);			");
		AddLine2Str(&FS_Source, "			vec3 Right = cross(Forward,vec3(0,1,0));				");
		AddLine2Str(&FS_Source, "			vec3 Up = cross(Forward,Right);							");
		AddLine2Str(&FS_Source, "			return CamLoc + Right*ScreenDisp.x + Up*ScreenDisp.y;   ");
		AddLine2Str(&FS_Source, "		}														    ");
		AddLine2Str(&FS_Source, "																	");
		AddLine2Str(&FS_Source, "		vec3 RayStartDir()											");
		AddLine2Str(&FS_Source, "		{															");
		AddLine2Str(&FS_Source, "			vec3 Start = CamLoc -CamPointAt*ScreenDimensions.z		");
		AddLine2Str(&FS_Source, "			return normalize(RayStartLoc-Start);					");
		AddLine2Str(&FS_Source, "		}															");
		AddLine2Str(&FS_Source, "																	");
		//AddLine2Str(&FS_Source, "		float[NUMBER] Distances										"); //Insert Field
		AddLine2Str(&FS_Source, "																	");


		FS_S1 = FS_Source;
		VS_S1 = VS_Source;
		FS_Source.insert(fFieldLoc, F_FieldCode);

		CameraLensePlane = OGL_Manager->AddFullscreenQuadGeometryCam(0,appBasics->ScreenSize);
		//fDistanceShader = OGL_Manager->AddShaders("./Shaders/VS_fDist.vert", "./Shaders/FS_fDist.frag");
		fDistanceShader = OGL_Manager->AddShadersViaText(VS_Source, FS_Source);
		return true;
	}
	else
	{
		return false;
	}
}