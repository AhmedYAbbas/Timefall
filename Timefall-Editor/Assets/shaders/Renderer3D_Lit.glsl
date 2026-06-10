//--------------------------
// - Timefall -
// Renderer3D Lit Shader (Phase 9.1 — normal matrix + entity-id; light still hardcoded)
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec3 u_CameraPosition;
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;

void main()
{
	vec4 world = u_Model * vec4(a_Position, 1.0);
	v_WorldPos = world.xyz;
	v_Normal = u_NormalMatrix * a_Normal;
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * world;
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec3 u_CameraPosition;
};

uniform int u_EntityID;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

// Hardcoded directional light + material (replaced by LightComponent + materials in 9.2/9.3).
const vec3  c_LightDir        = normalize(vec3(-0.5, -1.0, -0.3));
const vec3  c_LightColor      = vec3(1.0);
const vec3  c_MaterialDiffuse = vec3(0.8, 0.3, 0.3);
const vec3  c_MaterialSpecular= vec3(0.5);
const float c_Shininess       = 32.0;
const float c_AmbientStrength  = 0.1;

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 L = normalize(-c_LightDir);
	vec3 V = normalize(u_CameraPosition - v_WorldPos);
	vec3 H = normalize(L + V);

	float diff = max(dot(N, L), 0.0);
	float spec = pow(max(dot(N, H), 0.0), c_Shininess);

	vec3 ambient  = c_AmbientStrength * c_MaterialDiffuse;
	vec3 diffuse  = diff * c_MaterialDiffuse * c_LightColor;
	vec3 specular = spec * c_MaterialSpecular * c_LightColor;

	o_Color = vec4(ambient + diffuse + specular, 1.0);
	o_EntityID = u_EntityID;
}
