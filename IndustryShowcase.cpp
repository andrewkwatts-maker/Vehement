#include "IndustryShowcase.h"
#include "Camera.h"


IndustryShowcase::IndustryShowcase()
{

}

bool IndustryShowcase::update()
{
	return Application::update();
};
void IndustryShowcase::draw()
{

	OGL_Manager->UseShader(ShaderProgram);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());

	OGL_Manager->PassInUniform("camloc", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("depth_step_shift", 1/(D_Steps*10));
	OGL_Manager->PassInUniform("waterHeight", W_Level/100.0f);
	OGL_Manager->PassInUniform("LerpPow", L_Pow);

	OGL_Manager->SetTexture(TextureDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(TextureNormal, 1, "normal");
	OGL_Manager->SetTexture(TextureHeightmap, 2, "heightmap");
	OGL_Manager->SetTexture(TextureRoughness, 3, "roughness");
	OGL_Manager->SetTexture(TextureWaterDiffuse, 4, "diffuse2");
	OGL_Manager->SetTexture(TextureWaterNormal, 5, "normal2");

	OGL_Manager->DrawCustomGeometry(CustomPlain, vec3(0, 1, 0));
	TwDraw();
	Application::draw();

	//Gizmos::addLine(vec3(5, 0, -5), vec3())

};


bool IndustryShowcase::startup()
{
	if (Application::startup())
	{
		//Menu
		Menu = TwNewBar("Parralex - Andrew W");
		D_Steps = 100;
		W_Level = 1.0f;
		TwAddVarRW(Menu, "DepthStep", TW_TYPE_FLOAT, &D_Steps,"");
		TwAddVarRW(Menu, "Waterlevel", TW_TYPE_FLOAT, &W_Level, "");
		TwAddVarRW(Menu, "LerpPow", TW_TYPE_FLOAT, &L_Pow, "");


		vector<VertexComplex> VertexData;
		VertexData.push_back({ -5, 0, 5, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 2 });
		VertexData.push_back({ 5, 0, 5, 1, 0, 1, 0, 0, 1, 0, 0, 0, 2, 2 });
		VertexData.push_back({ 5, 0, -5, 1, 0, 1, 0, 0, 1, 0, 0, 0, 2, 0 });
		VertexData.push_back({ -5, 0, -5, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 });

		vector<unsigned int> indexData;
		indexData.push_back(0);
		indexData.push_back(1);
		indexData.push_back(2);
		indexData.push_back(0);
		indexData.push_back(2);
		indexData.push_back(3);

		CustomPlain = OGL_Manager->AddCustomGeometry(VertexData, indexData);

		TextureDiffuse = OGL_Manager->AddTexture("./data/textures/RWD3.jpg");
		TextureNormal = OGL_Manager->AddTexture("./data/textures/RWN.jpg");
		TextureHeightmap = OGL_Manager->AddTexture("./data/textures/RWH.jpg");
		TextureRoughness = OGL_Manager->AddTexture("./data/textures/RWN.png");
		TextureWaterDiffuse = OGL_Manager->AddTexture("./data/textures/WaterCD.png");
		TextureWaterNormal = OGL_Manager->AddTexture("./data/textures/WaterCN.jpg");

		ShaderProgram = OGL_Manager->AddShaders("./Shaders/VS_Parralex.vert", "./Shaders/FS_Parralex.frag");

		return true;
	}
	else
	{
		return false;
	}
}

IndustryShowcase::~IndustryShowcase()
{
}
