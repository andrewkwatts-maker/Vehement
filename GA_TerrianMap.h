#pragma once
#include "GL_Manager.h"

class MapTile
{
public:
	MapTile();
	bool Flat;
	float Heights[4]; //0 = X0,Y0, 1 = X1Y0, 2 = X0Y1, 3 = X1Y1
	int Xloc;
	int Yloc;
	float Freq;
	float Amp;
	glm::vec3 Normals[4];
	glm::vec3 Tangents[4];

	bool NeedsNewData(float Frequency, float Amplitude, int X, int Y);

	void GenData(float Frequency,float Amplitude,int X,int Y);
	glm::vec3 GetNormal (float Frequency, float Amplitude, int X, int Y);
	static float GetPerlin(float Frequency, float Amplitude, int X, int Y);
};

#include "Expandable3D_deque.h"

class GA_TerrianMap
{
public:
	Expandable3D_deque<MapTile>* Map;

	GA_TerrianMap(glm::vec3 CameraLoc);
	~GA_TerrianMap();

	int VissibleRange; // how far away from camera terrain should be generated;
	int UpdateDeltaRequirment; // how much data must be missing before updating map Space;
	float AdjustmentLeniancy; //multiple before its worth deleting.

	bool AutoUpdate;
	bool Updated;

	bool UpdateMapSpace(glm::vec3 CameraLoc,float Freq,float Amp); // returns true if different;

	float LastFreq;
	float LastAmp;

	void Draw(GL_Manager* Manager);

private:
	UIVec4 GLBufferID;

	bool UpdateRequired(glm::vec3 CameraLoc);
	bool HasGLBuffers;
	void CreateGL_Buffers();
	void DeleteGL_Buffers();
};

