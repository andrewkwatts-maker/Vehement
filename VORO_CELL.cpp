#include "VORO_CELL.h"
#include "Vertex.h"
#include <loadgen/gl_core_4_4.h>

VORO_CELL::VORO_CELL(vec3 _Loc, float _scale, VoroType Type) :VORO_CELL_CALCULATOR(_Loc,_scale,Type)
{
	HasGLBuffers = false;
}


VORO_CELL::~VORO_CELL()
{
	if (HasGLBuffers == true)
		Delete_GL_Buffers();

	for (int i = 0; i < Faces.size(); ++i)
	{
		for (int j = 0; j < Faces[i]->Edges.size(); ++j)
		{
			delete Faces[i]->Edges[j];
			Faces[i]->Edges.erase(Faces[i]->Edges.begin() + j);
			--j;
		}
		delete Faces[i];
		Faces.erase(Faces.begin() + i);
		--i;
	}
	
}

void VORO_CELL::Draw()
{
	if (Type > VoroType::eVOID)
	{
		for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
		{
			glBindVertexArray(VAO[Face]);
			glDrawElements(GL_TRIANGLES, GenBuffer_EdgeCount[Face] * 3, GL_UNSIGNED_INT, nullptr);
		}
	}
}

void VORO_CELL::Gen_GL_Buffers()
{
	CleanUp();

	GenBuffer_FaceCount = Faces.size();
	VBO = new unsigned int[GenBuffer_FaceCount];
	VAO = new unsigned int[GenBuffer_FaceCount];
	IBO = new unsigned int[GenBuffer_FaceCount];
	GenBuffer_EdgeCount = new unsigned int[GenBuffer_FaceCount];
	float TexScale = 2;

	for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
	{
		GenBuffer_EdgeCount[Face] = Faces[Face]->Edges.size();
		vec3 AveragePoint;
		vec3 Normal = Faces[Face]->Face->Normal;
		vec3 Tangent = glm::normalize(Faces[Face]->Edges[0]->BoundPos - Faces[Face]->Edges[0]->BoundNeg);

		//Define AveragePoint;
		for (int Edge = 0; Edge < GenBuffer_EdgeCount[Face]; ++Edge)
		{
			AveragePoint += Faces[Face]->Edges[Edge]->BoundPos;
			AveragePoint += Faces[Face]->Edges[Edge]->BoundNeg;
		}
		AveragePoint /= (2 * GenBuffer_EdgeCount[Face]);

		//GenAnArray;
		VertexComplex* Array;
		Array = new VertexComplex[GenBuffer_EdgeCount[Face]* 3];
		for (int Edge = 0; Edge <GenBuffer_EdgeCount[Face]; ++Edge)
		{
			vec3 P = Faces[Face]->Edges[Edge]->BoundPos;
			vec3 N = Faces[Face]->Edges[Edge]->BoundNeg;
			
			//PointA;
			Array[Edge * 3 + 0] = {P.x, P.y, P.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, P.x / TexScale, P.z / TexScale };
			//PointB;
			Array[Edge * 3 + 1] = {N.x, N.y, N.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, N.x / TexScale, N.z / TexScale };
			//PointC;
			Array[Edge * 3 + 2] = { AveragePoint.x, AveragePoint.y, AveragePoint.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, AveragePoint.x / TexScale, AveragePoint.z / TexScale };

		}
		unsigned int* IndexData;
		IndexData = new unsigned int[GenBuffer_EdgeCount[Face] * 3];
		for (int VertexID = 0; VertexID < GenBuffer_EdgeCount[Face] * 3; ++VertexID)
		{
			IndexData[VertexID] = VertexID;
		}

		glGenVertexArrays(1, &VAO[Face]);
		glBindVertexArray(VAO[Face]);

		glGenBuffers(1, &VBO[Face]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[Face]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex)*GenBuffer_EdgeCount[Face] * 3, Array, GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

		glGenBuffers(1, &IBO[Face]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO[Face]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*GenBuffer_EdgeCount[Face] * 3, IndexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

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

		//deleteArray;
		delete Array;
		delete IndexData;

		HasGLBuffers = true;
	}
}
void VORO_CELL::Delete_GL_Buffers()
{
	for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
	{
		glDeleteVertexArrays(1, &VAO[Face]);
		glDeleteBuffers(1, &VBO[Face]);
		glDeleteBuffers(1, &IBO[Face]);
	}
	delete [] VBO;
	delete [] VAO;
	delete [] IBO;
	delete [] GenBuffer_EdgeCount;

	HasGLBuffers = false;
}

void VORO_CELL::Delete_LeaveSeed()
{
	if (HasGLBuffers)
		Delete_GL_Buffers();
	for (int Face = 0; Face < Faces.size(); Face++)
	{
		delete Faces[Face];
		Faces.erase(Faces.begin());
		Face--;
	}
	
	
	bool ResultA = false;
};