//--------------------------
// - Timefall -
// Renderer3D Shadow Depth Shader (Phase A.0 — renders scene depth from the light)
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;    // unused; kept so the mesh VAO layout matches
layout(location = 2) in vec2 a_TexCoord;  // unused

uniform mat4 u_LightSpaceMatrix;  // lightProj * lightView
uniform mat4 u_Model;

void main()
{
	gl_Position = u_LightSpaceMatrix * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

void main()
{
	// Depth-only: the GPU writes gl_FragCoord.z into the depth array layer automatically.
}
