#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	mat4 u_View;
	vec3 u_CameraPosition;
};

uniform float u_EnvRotation;

layout(location = 0) out vec3 v_Dir;

vec3 rotateY(vec3 v, float a)
{
	float c = cos(a), s = sin(a);
	return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

void main()
{
	v_Dir = rotateY(a_Position, u_EnvRotation);

	mat4 rotView = mat4(mat3(u_View));                 // strip translation
	mat4 proj    = u_ViewProjection * inverse(u_View); // recover projection
	vec4 clip    = proj * rotView * vec4(a_Position, 1.0);
	gl_Position  = clip.xyww;                          // z = w -> depth 1.0
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_Dir;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

uniform samplerCube u_Skybox;

void main()
{
	o_Color = vec4(texture(u_Skybox, normalize(v_Dir)).rgb, 1.0);
	o_EntityID = -1;
}
