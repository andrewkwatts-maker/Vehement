#pragma once
#include <deque>
using namespace std;

template<class Type>
class Expandable2D_deque
{
public:
	deque<deque<Type*>> Map_XY;

	void Add_MinX();
	void Sub_MinX();
	void Add_MaxX();
	void Sub_MaxX();

	void Add_MinY();
	void Sub_MinY();
	void Add_MaxY();
	void Sub_MaxY();

	int GetMaxX(); //Exclusive
	int GetMinX(); //Inclusive
	int GetMaxY(); //Exclusive
	int GetMinY(); //Inclusive


	int GetSizeX(); //Number Of Elliments in X Dir
	int GetSizeY(); //Number Of Elliments in Y Dir


	bool ValidLoc(int X, int Y); //Checks if Valid Access Location

	//Safer Access
	Type* GetAt(int X, int Y);
	bool SetAt(int X, int Y, Type* Value);

	Type* GetAtQuick(int X, int Y);
	void SetAtQuick(int X, int Y, Type* Value);

	Expandable2D_deque(int StartX, int StartY);
	~Expandable2D_deque();

private:
	int Xmin; //Inclusive
	int Ymin; //Inclusive

	int SizeX; //Number Of Elliments
	int SizeY; //Number Of Elliments
};

