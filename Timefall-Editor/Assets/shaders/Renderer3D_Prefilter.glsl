#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
uniform mat4 u_ViewProjection;
layout(location = 0) out vec3 v_LocalPos;

void main()
{
	v_LocalPos = a_Position;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_LocalPos;
layout(location = 0) out vec4 o_Color;

uniform samplerCube u_EnvMap;
uniform float u_Roughness;
uniform float u_EnvResolution;   // face size of source env cubemap mip0

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;
const float MIP_BIAS = 2.0;  // extra pre-blur; kills sun-firefly variance FIS can't (measured p95 err 111% -> 3%)

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n) { return vec2(float(i) / float(n), RadicalInverse_VdC(i)); }

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	vec3 up    = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float D_GGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float d = (NdotH * a2 - NdotH) * NdotH + 1.0;
	return a2 / (PI * d * d);
}

void main()
{
	vec3 N = normalize(v_LocalPos);
	vec3 V = N;                       // Karis split-sum assumption: view = normal = reflection

	vec3 prefiltered = vec3(0.0);
	float totalWeight = 0.0;

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H  = ImportanceSampleGGX(Xi, N, u_Roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
		if (NdotL <= 0.0)
			continue;

		// Sample the env by solid angle -> mip level, to reduce fireflies (Karis / Colbert).
		float NdotH = max(dot(N, H), 0.0);
		float D = D_GGX(NdotH, u_Roughness);
		float pdf = (D * NdotH / (4.0 * max(dot(H, V), 1e-4))) + 1e-4;
		float saTexel  = 4.0 * PI / (6.0 * u_EnvResolution * u_EnvResolution);
		float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 1e-4);
		float mip = u_Roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel) + MIP_BIAS;

		prefiltered += textureLod(u_EnvMap, L, mip).rgb * NdotL;
		totalWeight += NdotL;
	}

	o_Color = vec4(prefiltered / max(totalWeight, 1e-4), 1.0);
}
