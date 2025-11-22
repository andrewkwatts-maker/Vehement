#pragma once
#include <deque>
#include <vector>
using namespace std;



template<class Type>
class Expandable3D_deque
{
public:
	deque<deque<deque<Type*>>> Map_XYZ;
	Type* NullResult;
	//Add Expands the Space in the direction Indicated
	//Sub Collapses the Space in the direction Indicated

	void Add_MinX();
	void Sub_MinX();
	void Add_MaxX();
	void Sub_MaxX();

	void Add_MinY();
	void Sub_MinY();
	void Add_MaxY();
	void Sub_MaxY();

	void Add_MinZ();
	void Sub_MinZ();
	void Add_MaxZ();
	void Sub_MaxZ();

	int GetMaxX(); //Exclusive
	int GetMinX(); //Inclusive
	int GetMaxY(); //Exclusive
	int GetMinY(); //Inclusive
	int GetMaxZ(); //Exclusive
	int GetMinZ(); //Inclusive

	int GetSizeX(); //Number Of Elliments in X Dir
	int GetSizeY(); //Number Of Elliments in Y Dir
	int GetSizeZ(); //Number Of Elliments in Z Dir

	bool ValidLoc(int X, int Y, int Z); //Checks if Valid Access Location

	//Safer Access
	Type* GetAt(int X, int Y, int Z);
	bool SetAt(int X, int Y, int Z, Type* Value);

	vector<Type*> GetShellAt(int X, int Y, int Z, int Dist); // Dist  == 0 returns GetAt, Dist == 1 returns GetCubeAt, Dist > 1 returns the shell.
	vector<Type*> GetCubeAt(int X, int Y, int Z);
	vector<Type*> GetRange(int Xmin, int Ymin, int Zmin, int Xmax, int Ymax, int Zmax);

	//Unsafe Access
	Type* GetAtQuick(int X, int Y, int Z);
	void SetAtQuick(int X, int Y, int Z, Type* Value);

	Expandable3D_deque(); //StartX = 0; StartY = 0; StartZ = 0;
	Expandable3D_deque(int StartX,int StartY,int StartZ);
	~Expandable3D_deque();
private:
	int Xmin; //Inclusive
	int Ymin; //Inclusive
	int Zmin; //inclusive

	int SizeX; //Number Of Elliments
	int SizeY; //Number Of Elliments
	int SizeZ; //Number Of Elliments
};


template<class Type>
Expandable3D_deque<Type>::Expandable3D_deque()
{
	Xmin = 0;
	Ymin = 0;
	Zmin = 0;

	SizeX = 0;
	SizeY = 0;
	SizeZ = 0;
}

template<class Type>
Expandable3D_deque<Type>::Expandable3D_deque(int StartX, int StartY, int StartZ)
{
	Xmin = StartX;
	Ymin = StartY;
	Zmin = StartZ;

	SizeX = 0;
	SizeY = 0;
	SizeZ = 0;

	NullResult = nullptr;
}

template<class Type>
Expandable3D_deque<Type>::~Expandable3D_deque(){

}

//X Add and Sub
template<class Type>
void Expandable3D_deque<Type>::Add_MinX(){
	deque<deque<Type*>> New;
	for (int Y = 0; Y < SizeY; ++Y)
	{
		deque<Type*> New2;
		for (int Z = 0; Z < SizeZ; ++Z)
		{
			Type* New3 = new Type();
			New2.push_back(New3);
		}
		New.push_back(New2);
	}
	Map_XYZ.push_front(New);
	++SizeX;
	--Xmin;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MinX(){
	if (SizeX > 0)
	{
		int X = 0;
		for (int Y = 0; Y < SizeY; ++Y)
		{
			for (int Z = 0; Z < SizeZ; ++Z)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}
		Map_XYZ.pop_front();
		--SizeX;
		++Xmin;
	}
}

template<class Type>
void Expandable3D_deque<Type>::Add_MaxX(){
	deque<deque<Type*>> New;
	for (int Y = 0; Y < SizeY; ++Y)
	{
		deque<Type*> New2;
		for (int Z = 0; Z < SizeZ; ++Z)
		{
			Type* New3 = new Type();
			New2.push_back(New3);
		}
		New.push_back(New2);
	}
	Map_XYZ.push_back(New);
	++SizeX;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MaxX(){
	if (SizeX > 0)
	{
		int X = SizeX - 1;
		for (int Y = 0; Y < SizeY; ++Y)
		{
			for (int Z = 0; Z < SizeZ; ++Z)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}
		Map_XYZ.pop_back();
		--SizeX;
	}
}


//Y Add and Sub
template<class Type>
void Expandable3D_deque<Type>::Add_MinY(){
	for (int X = 0; X < SizeX; ++X)
	{
		deque<Type*> New;
		for (int Z = 0; Z < SizeZ; ++Z)
		{
			Type* New2 = new Type();
			New.push_back(New2);
		}
		Map_XYZ[X].push_front(New);
	}
	++SizeY;
	--Ymin;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MinY(){
	if (SizeY > 0)
	{
		for (int X = 0; X < SizeX; ++X)
		{
			int Y = 0;
			for (int Z = 0; Z < SizeZ; ++Z)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}

		for (int X = 0; X < SizeX; ++X)
		{
			Map_XYZ[X].pop_front();
		}
		--SizeY;
		++Ymin;
	}
}
template<class Type>
void Expandable3D_deque<Type>::Add_MaxY(){
	for (int X = 0; X < SizeX; ++X)
	{
		deque<Type*> New;
		for (int Z = 0; Z < SizeZ; ++Z)
		{
			Type* New2 = new Type();
			New.push_back(New2);
		}
		Map_XYZ[X].push_back(New);
	}
	++SizeY;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MaxY(){
	if (SizeY > 0)
	{
		for (int X = 0; X < SizeX; ++X)
		{
			int Y = SizeY-1;
			for (int Z = 0; Z < SizeZ; ++Z)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}

		for (int X = 0; X < SizeX; ++X)
		{
			Map_XYZ[X].pop_back();
		}
		--SizeY;
	}
}



//Z Add and Sub
template<class Type>
void Expandable3D_deque<Type>::Add_MinZ(){
	for (int X = 0; X < SizeX; ++X)
	{
		for (int Y = 0; Y < SizeY; ++Y)
		{
			Type* New = new Type();
			Map_XYZ[X][Y].push_front(New);
		}
	}
	++SizeZ;
	--Zmin;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MinZ(){
	if (SizeZ > 0)
	{
		int Z = 0;
		for (int X = 0; X < SizeX; ++X)
		{
			for (int Y = 0; Y < SizeY; ++Y)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}

		for (int X = 0; X < SizeX; ++X)
		{
			for (int Y = 0; Y < SizeY; ++Y)
			{
				Map_XYZ[X][Y].pop_front();
			}
		}
		--SizeZ;
		++Zmin;
	}
}
template<class Type>
void Expandable3D_deque<Type>::Add_MaxZ(){
	for (int X = 0; X < SizeX; ++X)
	{
		for (int Y = 0; Y < SizeY; ++Y)
		{
			Type* New = new Type();
			Map_XYZ[X][Y].push_back(New);
		}
	}
	++SizeZ;
}
template<class Type>
void Expandable3D_deque<Type>::Sub_MaxZ(){
	if (SizeZ > 0)
	{
		int Z = SizeZ-1;
		for (int X = 0; X < SizeX; ++X)
		{
			for (int Y = 0; Y < SizeY; ++Y)
			{
				delete Map_XYZ[X][Y][Z];
			}
		}


		for (int X = 0; X < SizeX; ++X)
		{
			for (int Y = 0; Y < SizeY; ++Y)
			{
				Map_XYZ[X][Y].pop_back();
			}
		}
		--SizeZ;
	}
}

template<class Type>
Type* Expandable3D_deque<Type>::GetAt(int X, int Y, int Z)
{
	int RefX = X - Xmin;
	int RefY = Y - Ymin;
	int RefZ = Z - Zmin;

	if (RefX >= 0 && RefY >= 0 && RefZ >= 0)
	{
		if (RefX < SizeX && RefY < SizeY && RefZ < SizeZ)
			return Map_XYZ[RefX][RefY][RefZ];
	}
	return NullResult;
}

template<class Type>
vector<Type*> Expandable3D_deque<Type>::GetCubeAt(int X, int Y, int Z)
{
	vector<Type*> Results;

	//Center
	if (ValidLoc(X,Y,Z))
		Results.push_back(GetAtQuick(X, Y, Z));
	

	//First Grouping
	if (ValidLoc(X-1, Y, Z))
		Results.push_back(GetAtQuick(X-1, Y, Z));
	if (ValidLoc(X + 1, Y, Z))
		Results.push_back(GetAtQuick(X + 1, Y, Z));

	if (ValidLoc(X, Y-1, Z))
		Results.push_back(GetAtQuick(X, Y-1, Z));
	if (ValidLoc(X, Y + 1, Z))
		Results.push_back(GetAtQuick(X, Y + 1, Z));
	
	if (ValidLoc(X,Y,Z-1))
		Results.push_back(GetAtQuick(X, Y, Z-1));
	if (ValidLoc(X, Y, Z + 1))
		Results.push_back(GetAtQuick(X, Y, Z + 1));

	//Secound Grouping
	if (ValidLoc(X - 1, Y, Z-1))
		Results.push_back(GetAtQuick(X - 1, Y, Z-1));
	if (ValidLoc(X + 1, Y, Z-1))
		Results.push_back(GetAtQuick(X + 1, Y, Z-1));
	if (ValidLoc(X - 1, Y, Z + 1))
		Results.push_back(GetAtQuick(X - 1, Y, Z + 1));
	if (ValidLoc(X + 1, Y, Z + 1))
		Results.push_back(GetAtQuick(X + 1, Y, Z + 1));

	if (ValidLoc(X-1, Y - 1, Z))
		Results.push_back(GetAtQuick(X-1, Y - 1, Z));
	if (ValidLoc(X-1, Y + 1, Z))
		Results.push_back(GetAtQuick(X-1, Y + 1, Z));
	if (ValidLoc(X + 1, Y - 1, Z))
		Results.push_back(GetAtQuick(X + 1, Y - 1, Z));
	if (ValidLoc(X + 1, Y + 1, Z))
		Results.push_back(GetAtQuick(X + 1, Y + 1, Z));

	if (ValidLoc(X, Y-1, Z - 1))
		Results.push_back(GetAtQuick(X, Y-1, Z - 1));
	if (ValidLoc(X, Y-1, Z + 1))
		Results.push_back(GetAtQuick(X, Y-1, Z + 1));
	if (ValidLoc(X, Y + 1, Z - 1))
		Results.push_back(GetAtQuick(X, Y+1, Z - 1));
	if (ValidLoc(X, Y + 1, Z + 1))
		Results.push_back(GetAtQuick(X, Y+1, Z + 1));


	//Thirdgrouping
	if (ValidLoc(X - 1, Y-1, Z - 1))
		Results.push_back(GetAtQuick(X - 1, Y-1, Z - 1));
	if (ValidLoc(X + 1, Y-1, Z - 1))
		Results.push_back(GetAtQuick(X + 1, Y-1, Z - 1));
	if (ValidLoc(X - 1, Y-1, Z + 1))
		Results.push_back(GetAtQuick(X - 1, Y-1, Z + 1));
	if (ValidLoc(X + 1, Y-1, Z + 1))
		Results.push_back(GetAtQuick(X + 1, Y-1, Z + 1));

	if (ValidLoc(X - 1, Y + 1, Z - 1))
		Results.push_back(GetAtQuick(X - 1, Y + 1, Z - 1));
	if (ValidLoc(X + 1, Y + 1, Z - 1))
		Results.push_back(GetAtQuick(X + 1, Y + 1, Z - 1));
	if (ValidLoc(X - 1, Y + 1, Z + 1))
		Results.push_back(GetAtQuick(X - 1, Y + 1, Z + 1));
	if (ValidLoc(X + 1, Y + 1, Z + 1))
		Results.push_back(GetAtQuick(X + 1, Y + 1, Z + 1));

	return Results;




}

template<class Type>
vector<Type*> Expandable3D_deque<Type>::GetShellAt(int X, int Y, int Z, int Dist)
{
	vector<Type*> Results;
	if (Dist == 0)
	{
		Results.push_back(GetAt(X, Y, Z));
	}
	else if (Dist == 1)
	{
		Results = GetCubeAt(X, Y, Z);
	}
	else if (Dist > 1)
	{
		int Xm = X - Dist;
		int Ym = Y - Dist;
		int Zm = Z - Dist;
		int XM = X + Dist;
		int YM = Y + Dist;
		int ZM = Z + Dist;

		//Base
		vector<Type*> SideYN = GetRange(Xm, Ym, Zm, XM, Ym, ZM);
		vector<Type*> SideYP = GetRange(Xm, YM, Zm, XM, YM, ZM); //Largest

		vector<Type*> SideXP = GetRange(Xm, Ym+1, Zm, Xm, YM-1, ZM);
		vector<Type*> SideXN = GetRange(XM, Ym + 1, Zm, XM, YM - 1, ZM); //2nd Smallest

		vector<Type*> SideZP = GetRange(Xm+1, Ym + 1, ZM, XM-1, YM - 1, ZM); //Smallest
		vector<Type*> SideZN = GetRange(Xm+1, Ym + 1, Zm, XM-1, YM - 1, Zm);

		int SizeZ = SideZP.size();
		int SizeX = SideXP.size();
		int SizeY = SideYP.size();

		for (int i = 0; i < SizeZ; ++i)
		{
			Results.push_back(SideZN[i]);
			Results.push_back(SideZP[i]);
			Results.push_back(SideYN[i]);
			Results.push_back(SideYP[i]);
			Results.push_back(SideXN[i]);
			Results.push_back(SideXP[i]);
		}
		for (int i = SizeZ; i < SizeX; ++i)
		{
			Results.push_back(SideYN[i]);
			Results.push_back(SideYP[i]);
			Results.push_back(SideXN[i]);
			Results.push_back(SideXP[i]);
		}
		for (int i = SizeX; i < SizeY; ++i)
		{
			Results.push_back(SideYN[i]);
			Results.push_back(SideYP[i]);
		}

	}
	return Results;
}

template<class Type>
vector<Type*> Expandable3D_deque<Type>::GetRange(int Xmin, int Ymin, int Zmin, int Xmax, int Ymax, int Zmax)
{
	vector<Type*> Results;
	for (int X = Xmin; X <= Xmax; ++X)
	{
		for (int Y = Ymin; Y <= Ymax; ++Y)
		{
			for (int Z = Zmin; Z <= Zmax; ++Z)
			{
				Results.push_back(GetAt(X, Y, Z));
			}
		}
	}
	return Results;
}


template<class Type>
bool Expandable3D_deque<Type>::SetAt(int X, int Y, int Z, Type* Value)
{
	int RefX = X - Xmin;
	int RefY = Y - Ymin;
	int RefZ = Z - Zmin;

	if (RefX >= 0 && RefY >= 0 && RefZ >= 0)
	{
		if (RefX < SizeX && RefY < SizeY && RefZ < SizeZ)
		{
			Map_XYZ[RefX][RefY][RefZ] = Value;
			return true;
		}
	}
	return false;
}


template<class Type>
Type* Expandable3D_deque<Type>::GetAtQuick(int X, int Y, int Z)
{
	return Map_XYZ[X - Xmin][Y - Ymin][Z - Zmin];
}

template<class Type>
void Expandable3D_deque<Type>::SetAtQuick(int X, int Y, int Z, Type* Value)
{
	Map_XYZ[X - Xmin][Y - Ymin][Z - Zmin] = Value;
}




template<class Type>
bool Expandable3D_deque<Type>::ValidLoc(int X, int Y, int Z)
{
	int RefX = X - Xmin;
	int RefY = Y - Ymin;
	int RefZ = Z - Zmin;

	if (RefX >= 0 && RefY >= 0 && RefZ >= 0)
	{
		if (RefX < SizeX && RefY < SizeY && RefZ < SizeZ)
		{
			return true;
		}
	}
	return false;
}


template<class Type>
int Expandable3D_deque<Type>::GetMaxX()
{
	return Xmin + SizeX;
}
template<class Type>
int Expandable3D_deque<Type>::GetMinX()
{
	return Xmin;
}
template<class Type>
int Expandable3D_deque<Type>::GetMaxY()
{
	return Ymin + SizeY;
}
template<class Type>
int Expandable3D_deque<Type>::GetMinY()
{
	return Ymin;
}
template<class Type>
int Expandable3D_deque<Type>::GetMaxZ()
{
	return Zmin + SizeZ;
}
template<class Type>
int Expandable3D_deque<Type>::GetMinZ()
{
	return Zmin;
}

template<class Type>
int Expandable3D_deque<Type>::GetSizeX()
{
	return SizeX;
}
template<class Type>
int Expandable3D_deque<Type>::GetSizeY()
{
	return SizeY;
}
template<class Type>
int Expandable3D_deque<Type>::GetSizeZ()
{
	return SizeZ;
}

