//--------------------------
// - Timefall -
// Renderer3D Lit Shader (Phase 9.2 — directional/point/spot Blinn-Phong; material hardcoded)
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

struct DirLight   { vec4 Direction; vec4 Color; };                          // Color.a = intensity
struct PointLight { vec4 Position;  vec4 Color; };                          // Position.w = range, Color.a = intensity
struct SpotLight  { vec4 Position;  vec4 Direction; vec4 Color; vec4 Params; };
// Spot: Position.xyz = position, Direction.xyz = direction, Color.rgb = color,
//       Params = (range, innerCos, outerCos, intensity)

const int MAX_DIR   = 4;
const int MAX_POINT = 32;
const int MAX_SPOT  = 16;

layout(std140, binding = 1) uniform Lights
{
	DirLight   u_DirLights[MAX_DIR];
	PointLight u_PointLights[MAX_POINT];
	SpotLight  u_SpotLights[MAX_SPOT];
	int u_DirCount;
	int u_PointCount;
	int u_SpotCount;
};

uniform int u_EntityID;

uniform vec3  u_DiffuseColor;
uniform vec3  u_SpecularColor;
uniform float u_Shininess;
uniform sampler2D u_DiffuseMap;
uniform sampler2D u_SpecularMap;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

const float c_AmbientStrength = 0.09f;

// Blinn-Phong contribution of one light. L = surface->light (unit). radiance already folds in
// the light color, intensity, and any attenuation/cone factor.
vec3 BlinnPhong(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 matDiffuse, vec3 matSpecular, float shininess)
{
	float diff = max(dot(N, L), 0.0);
	vec3  H    = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), shininess);
	return (diff * matDiffuse + spec * matSpecular) * radiance;
}

// Range-based attenuation: smoothly 1 at the light, 0 at distance == range.
float Attenuate(float dist, float range)
{
	float x = clamp(1.0 - (dist * dist) / (range * range), 0.0, 1.0);
	return x * x;
}

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_CameraPosition - v_WorldPos);

	vec3 matDiffuse  = u_DiffuseColor  * texture(u_DiffuseMap,  v_TexCoord).rgb;
	vec3 matSpecular = u_SpecularColor * texture(u_SpecularMap, v_TexCoord).rgb;
	//vec3 matSpecular = vec3(0.4f);

	vec3 color = c_AmbientStrength * matDiffuse;

	// Directional
	for (int i = 0; i < u_DirCount; i++)
	{
		vec3 L = normalize(-u_DirLights[i].Direction.xyz);
		vec3 radiance = u_DirLights[i].Color.rgb * u_DirLights[i].Color.a;
		color += BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
	}

	// Point
	for (int i = 0; i < u_PointCount; i++)
	{
		vec3 toLight = u_PointLights[i].Position.xyz - v_WorldPos;
		float dist = length(toLight);
		vec3 L = toLight / max(dist, 0.0001);
		float att = Attenuate(dist, u_PointLights[i].Position.w);
		vec3 radiance = u_PointLights[i].Color.rgb * u_PointLights[i].Color.a * att;
		color += BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
	}

	// Spot
	for (int i = 0; i < u_SpotCount; i++)
	{
		float range     = u_SpotLights[i].Params.x;
		float innerCos  = u_SpotLights[i].Params.y;
		float outerCos  = u_SpotLights[i].Params.z;
		float intensity = u_SpotLights[i].Params.w;

		vec3 toLight = u_SpotLights[i].Position.xyz - v_WorldPos;
		float dist = length(toLight);
		vec3 L = toLight / max(dist, 0.0001);
		float att = Attenuate(dist, range);

		vec3  spotDir = normalize(u_SpotLights[i].Direction.xyz);
		float theta   = dot(-L, spotDir);                       // 1 when fragment is on the cone axis
		float cone    = clamp((theta - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);

		vec3 radiance = u_SpotLights[i].Color.rgb * intensity * att * cone;
		color += BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
	}

	o_Color = vec4(color, 1.0);
	//o_Color.rgb = pow(o_Color.rgb, vec3(1.0/2.2)); // Gamma correction
	o_EntityID = u_EntityID;
}
