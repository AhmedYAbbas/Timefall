import Common.Bindings;
import Common.BRDF;

static const float c_AmbientStrength = 0.09;

float3 EvaluateAmbient(SurfaceParams s)
{
	return c_AmbientStrength * s.Albedo * s.AO;
}
