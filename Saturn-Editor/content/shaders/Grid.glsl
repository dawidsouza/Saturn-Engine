#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(binding = 0) uniform Matrices 
{
	mat4 ViewProjection;
	
	mat4 Transform;
	float Scale;
	float Res;
} u_Matrices;

layout(location = 1) out VertexOutput 
{
	vec2 TexCoord;
} vs_Output;

void main() 
{
	vs_Output.TexCoord = a_TexCoord;
	
	gl_Position = u_Matrices.ViewProjection * u_Matrices.Transform * vec4( a_Position, 1.0 );	
}

#type fragment
#version 450

layout(location = 0) out vec4 FinalColor;
// The geo pass has two extra outputs that we don't write to.
layout(location = 1) out vec4 UnusedA; 
layout(location = 2) out vec4 UnusedB;

layout(binding = 0) uniform Matrices 
{
	mat4 ViewProjection;

	mat4 Transform;
	float Scale;
	float Res;
} u_Matrices;

layout(location = 1) in VertexOutput 
{
	vec2 TexCoord;
} vs_Input;

float grid( vec2 st ) 
{
	float res = u_Matrices.Res;
	
	vec2 grid = fract( st / 2 );

	return step( res, grid.x ) * step( res, grid.y );
}

void main()
{
	float scale = u_Matrices.Scale;
	float res = u_Matrices.Res;
	
	float x = grid( vs_Input.TexCoord * scale );
	
	vec4 Color = vec4( vec3( 0.2 ), 0.5 ) * ( 1.0 - x );

	FinalColor = Color;

	if( Color.a == 0.0 )
		discard;
}