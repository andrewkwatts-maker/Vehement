#version 410
in vec2	vTexCoord;
in vec4 Colour;
out vec4 fragColour;
uniform sampler2D diffuse;


void main(){
   	if(texture(diffuse,vTexCoord).a < 0.2f){
        discard;
    	}
	fragColour = Colour*texture(diffuse,vTexCoord);
}