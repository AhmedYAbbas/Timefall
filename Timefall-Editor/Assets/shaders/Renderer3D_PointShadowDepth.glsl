//--------------------------
// - Timefall -
// Renderer3D Point Shadow Depth Shader (Phase C — stores linear distance-to-light)
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;    // unused; kept so the mesh VAO layout matches
layout(location = 2) in vec2 a_TexCoord;  // unused

uniform mat4 u_LightSpaceMatrix;  // faceProj * faceView
uniform mat4 u_Model;

out vec3 v_WorldPos;

void main()
{
	vec4 world = u_Model * vec4(a_Position, 1.0);
	v_WorldPos = world.xyz;
	gl_Position = u_LightSpaceMatrix * world;
}

#type fragment
#version 450 core

in vec3 v_WorldPos;

uniform vec3  u_LightPos;
uniform float u_Far;

void main()
{
	gl_FragDepth = length(v_WorldPos - u_LightPos) / u_Far;
}
