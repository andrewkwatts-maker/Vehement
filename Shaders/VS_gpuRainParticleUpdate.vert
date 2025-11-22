#version 410

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Velocity;
layout(location = 2) in float Lifetime;
layout(location = 3) in float Lifespan;

out vec3 position;
out vec3 velocity;
out float lifetime;
out float lifespan;

uniform float time;
uniform float deltaTime;
uniform float lifeMin;
uniform float lifeMax;
uniform float velocityMin;
uniform float velocityMax;
uniform vec3 emitterPosition;
uniform vec3 lastPosition;

const float INVERSE_MAX_UINT = 1.0f/4294967295.0f;

float rand(uint seed,float range)
{
	uint i = (seed ^ 12345391u) * 2654435769u;
	i ^= (i << 6u) ^ (i >> 26u);
	i *= 2654435769u;
	i += (i << 5u) ^ (i >> 12u);
	return float(range*i) * INVERSE_MAX_UINT;
}

void main(){
	position = Position +Velocity*deltaTime;
	velocity  = Velocity;
	lifetime = Lifetime + deltaTime;
	lifespan = Lifespan;



	if(lifetime > lifespan)
	{
		uint seed = uint(time*1000.0) + uint(gl_VertexID);
		velocity.x = rand(seed++,2)-1;
		velocity.y = rand(seed++,2)-1;
		velocity.z = rand(seed++,2)-1;
		velocity = normalize(velocity)*(rand(seed++,velocityMax-velocityMin)+velocityMin);
		velocity = velocity + vec3(0,-40,0);
		float ratioa = rand(seed++,1);
		float ratiob = rand(seed++,1);
		float ratioc = rand(seed++,1);
		position = vec3(lastPosition.x*ratioa +emitterPosition.x*(1-ratioa),emitterPosition.y*ratioc + lastPosition.y*(1-ratioc),lastPosition.z*ratiob +emitterPosition.z*(1-ratiob));
	
		lifetime = 0;
		lifespan = rand(seed++,lifeMax-lifeMin) +lifeMin;
	}


	if(Position.y<lastPosition.y)
	{
		uint seed = uint(time*1000.0) + uint(gl_VertexID);
		velocity.x = rand(seed++,2)-1;
		velocity.y = rand(seed++,2)-1;
		velocity.z = rand(seed++,2)-1;
		velocity = normalize(velocity)*(rand(seed++,velocityMax-velocityMin)+velocityMin);
		velocity = velocity + vec3(0,-40,0);
		float ratioa = rand(seed++,1);
		float ratiob = rand(seed++,1);
		position = vec3(lastPosition.x*ratioa +emitterPosition.x*(1-ratioa),emitterPosition.y,lastPosition.z*ratiob +emitterPosition.z*(1-ratiob));
	
		lifetime = 0;
		lifespan = rand(seed++,lifeMax-lifeMin) +lifeMin;
	}
}