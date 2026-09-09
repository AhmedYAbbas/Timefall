import Common.Bindings;

// pixel is SV_Position.xy from the fragment stage: the per-pixel seed for the Vogel rotation
float SunShadow(float3 worldPos, float viewDepth, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}

float SpotShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}

float PointShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}
