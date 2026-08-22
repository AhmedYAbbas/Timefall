import Common.Bindings;


struct SurfaceParams
{
	float3 Albedo;
	float3 N;
	float3 V;
	float3 F0;
	float Roughness;
	float Metallic;
	float AO;
}

static const float c_AmbientStrength = 0.09;

float3 EvaluateAmbient(SurfaceParams s)
{
	return c_AmbientStrength * s.Albedo * s.AO;
}
