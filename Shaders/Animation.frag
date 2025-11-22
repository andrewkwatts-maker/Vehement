# version 410

//uniform float time;
//uniform vec2 gridSize;

uniform vec3 LightDir;

uniform sampler2D diffuse;
//uniform sampler2D tinting;
uniform sampler2D normal;

in vec3 frag_normal;
in vec3 frag_position;
in vec3 frag_tangent;
in vec3 frag_bitangent;
in vec2 frag_texcoord;

out vec4 FragColor;

void main()
{
	//vec2 textureCoord  = vec2(vPosition.x / gridSize.x, vPosition.z / gridSize.y);
	//FragColor = texture(diffuse,  frag_texcoord) /*  texture(tinting,  textureCoord)*/;

	mat3 TBN = mat3( normalize( frag_tangent ), normalize( frag_bitangent ), normalize( frag_normal ));  
	vec3 N = texture(normal, frag_texcoord).xyz * 2 - 1;
	float d = max( 0, dot( normalize( TBN * N ), normalize( LightDir )));
	FragColor = vec4(texture(diffuse, frag_texcoord).xyz, 1.0f);
	FragColor.rgb = FragColor.rgb * d;
}
