//--------------------------
// - Timefall -
// Cosine-weighted convolution of the skybox into a 32^2 diffuse irradiance cube. One texel is a
// full hemisphere integral, so this is the expensive half of the bake
// --------------------------

import Common.Bindings;

static const float PI = 3.14159265359;

struct IrradiancePush
{
	float4x4 ViewProjection;
	uint SkyboxIndex;
	uint _Pad0;
	uint _Pad1;
	uint _Pad2;
}

[[vk::push_constant]]
ConstantBuffer<IrradiancePush> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float3 localPos : TEXCOORD0;
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

	float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	const float3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	float3 irradiance = float3(0.0f);
	float sampleCount = 0.0f;
	const float dPhi = 0.025f;
	const float dTheta = 0.025f;

	for (float phi = 0.0f; phi < 2.0f * PI; phi += dPhi)
	{
		for (float theta = 0.0f; theta < 0.5f * PI; theta += dTheta)
		{
			const float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			const float3 dir = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

			irradiance += SampleBindlessCube(u_Push.SkyboxIndex, SAMPLER_LINEAR_CLAMP, dir).rgb * cos(theta) * sin(theta);
			sampleCount += 1.0f;
		}
	}

	return float4(PI * irradiance / sampleCount, 1.0f);
}
