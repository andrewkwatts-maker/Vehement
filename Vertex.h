#pragma once
#include <glm\glm.hpp>

using glm::vec4;
using glm::vec3;

class Vertex
{
public:
	vec4 Position;
	vec4 Colour;

	Vertex();
	~Vertex();
};

struct VertexBasicTextured
{
	float x, y, z, w; //Position
	float s, t; // TextureCords
};

struct VertexComplex
{
	float x, y, z, w; //Position
	float nx,ny,nz,nw; //Normals
	float tx,ty,tz,tw; //Tangents
	float s,t; // TextureCords
};

