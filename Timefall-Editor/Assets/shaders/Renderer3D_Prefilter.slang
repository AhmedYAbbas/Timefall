//--------------------------
// - Timefall -
// GGX importance-sampled prefilter of the skybox, one mip per roughness. Samples by solid angle
// so a small bright source does not alias into fireflies
// --------------------------

import Common.Bindings;
import Common.BRDF;

static const uint SAMPLE_COUNT = 1024u;
static const float MIP_BIAS = 2.0f; // extra pre-blur; kills sun-firefly variance FIS can't (measured p95 err 111% -> 3%)

struct PrefilterPush
{
	float4x4 ViewProjection;
	uint SkyboxIndex;
	float Roughness;
	float EnvResolution; // face size of the source cube's mip 0
	uint _Pad0;
}

[[vk::push_constant]]
ConstantBuffer<PrefilterPush> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float3 localPos : TEXCOORD0;
}

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint n)
{
	return float2(float(i) / float(n), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	const float a = roughness * roughness;
	const float phi = 2.0f * PI * Xi.x;
	const float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	const float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	const float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);

	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// All five MeshVertex attributes are declared because the pipeline validates the declared count
// against its vertex layout; only position is used
[shader("vertex")]
VSOutput vsMain(float3 position, float3 normal, float2 texCoord, float3 tangent, float3 bitangent)
{
	VSOutput o;
	o.localPos = position;
	o.position = mul(u_Push.ViewProjection, float4(position, 1.0f));

	return o;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target0
{
	const float3 N = normalize(input.localPos);
	const float3 V = N; // Karis split-sum assumption: view = normal = reflection

	float3 prefiltered = float3(0.0f);
	float totalWeight = 0.0f;

	for (uint i = 0u; i < SAMPLE_COUNT; i++)
	{
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
		const float3 H = ImportanceSampleGGX(Xi, N, u_Push.Roughness);
		const float3 L = normalize(2.0f * dot(V, H) * H - V);

		const float NdotL = max(dot(N, L), 0.0f);
		if (NdotL <= 0.0f)
			continue;

		// Sample the env by solid angle -> mip level, to reduce fireflies (Karis / Colbert)
		const float NdotH = max(dot(N, H), 0.0f);
		const float a = u_Push.Roughness * u_Push.Roughness;
		const float D = D_GGX(NdotH, a);
		const float pdf = (D * NdotH / (4.0f * max(dot(H, V), 1e-4f))) + 1e-4f;
		const float saTexel = 4.0f * PI / (6.0f * u_Push.EnvResolution * u_Push.EnvResolution);
		const float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 1e-4f);
		const float mip = u_Push.Roughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel) + MIP_BIAS;

		prefiltered += SampleBindlessCubeLod(u_Push.SkyboxIndex, SAMPLER_LINEAR_CLAMP, L, mip).rgb * NdotL;
		totalWeight += NdotL;
	}

	return float4(prefiltered / max(totalWeight, 1e-4f), 1.0f);
}
