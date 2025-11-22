#version 410
in vec2	vTexCoord;
out vec4 FragColor;

uniform vec3 CamLoc; //position of observation
uniform vec3 CamPointAt; //position of focus
uniform float FOV; //field of view angle
uniform float Tilt; //angle the camera is tilted on.
uniform float AspectRatio;
uniform float SphericalLensingRatio;
uniform int MaxSteps;


vec3 GetSceneVector(vec3 foward,vec3 up, vec3 right)
{
	float XAngle = vTexCoord.x*FOV;
	float YAngle = vTexCoord.y*FOV/AspectRatio;

	vec3 TiltedRight = cos(XAngle)*foward + sin(XAngle)*right;
	vec3 TiltedFinal = cos(YAngle)*TiltedRight + sin(YAngle)*up;
	
	vec3 TiltedRight1 = (1/tan(FOV))*foward + vTexCoord.x*right;
	vec3 TiltedFinal1 = (TiltedRight1 +vTexCoord.y/AspectRatio*up);

	return normalize(TiltedFinal*SphericalLensingRatio + (1-SphericalLensingRatio)*TiltedFinal1);
}

float v3Max(vec3 vector)
{
	return max(max(vector.x,vector.y),vector.z);
}

float fMakeSphere(vec3 Point,float Radius)
{
	return length(Point)-Radius;
}

float fMakeColumn(vec2 Point,float Radius)
{
	return length(Point)-Radius;
}

float fMakeBox(vec3 Point,vec3 Bounds)
{
	vec3 Basic = abs(Point) - Bounds;
	return length(max(Basic,vec3(0))) +v3Max(min(Basic,vec3(0)));
}

int fTileSpace(inout float Dimension,float TileSize)
{
	int Result = int(floor((Dimension+TileSize/2)/TileSize));
	Dimension = mod(Dimension+TileSize/2,TileSize) - TileSize/2;
	return Result;
	
}

float Subract(float A,float B)
{
	return max(A,-B);
}

float Add(float A,float B)
{
	return min(A,B);
}

float InCommon(float A,float B)
{
	return max(A,B);
}

void Shift(inout float Dimension,float Movement)
{
	Dimension = Dimension-Movement;
}

void Shift3(inout vec3 Space,vec3 Movement)
{
	Space = Space-Movement;
}

void Rotate(inout vec2 Plane,float Angle)
{
	Plane = cos(Angle)*Plane + sin(Angle)*vec2(Plane.y,-Plane.x);
}

void Mirror(inout float Dimension, float Distance)
{
	Dimension = abs(Dimension) - Distance; 
}

float fFunction(vec3 Loc)
{
	int Cell = fTileSpace(Loc.x,10);
	float Box = fMakeBox(Loc,vec3(2,3,4));
	float Column = fMakeColumn(Loc.xz,2);
	return Add(Box,Column);
}



vec3 circleTrace(vec3 Start,vec3 Dir,float InterceptDistance)
{
	vec3 Location = Start;
	int Step = 0;

	while(Step<MaxSteps)
	{
		float LengthToASurface = fFunction(Location);
		if(LengthToASurface<InterceptDistance)
		{
			return vec3(1,length(Location-Start),0);
		}
		else if(Location.y<0)
		{
			Location-=(Dir/Dir.y)*Location.y;
			LengthToASurface = fFunction(Location);
			return vec3(2,length(Location-Start),LengthToASurface);
		}
		else
		{
			Step+=1;
			Location+= Dir*LengthToASurface;
		};	
	};
	return vec3(0,length(Location-Start),0);
}


void main()
{
//CameraSetup

vec3 Foward = normalize(CamPointAt-CamLoc);
vec3 Right = normalize(cross(Foward,vec3(0,1,0)));
vec3 Up = cross(Right,Foward);

vec3 TiltedRight = (cos(Tilt)*Right + sin(Tilt)*Up);
vec3 TiltedUp = cross(TiltedRight,Foward);
vec3 FinalVector = GetSceneVector(Foward,TiltedUp, TiltedRight);



//TraceIntoField
vec3 TraceResult = circleTrace(CamLoc,FinalVector,0.01f);

//RespondToResult;
if(TraceResult.x == 1)
{
	FragColor = vec4(0.5f,0.5f,0.5f,1);
}
else if(TraceResult.x == 2)
{
	//FragColor = vec4(0,0,0,1);
	if(int(TraceResult.z*7)%7 == 0)
	{
		FragColor = vec4(0,0,0,1);
	}
	else
	{
		FragColor = vec4(1.0f,TraceResult.z/5,1.0f,1);
	}
}
else
{
	FragColor = vec4(0,0,0,1);
}

};


