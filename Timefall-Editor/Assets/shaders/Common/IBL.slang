import Bindings;
import BRDF;

static const float c_AmbientStrength = 0.09;

float3 RotateY(float3 v, float angle)
{
	const float c = cos(angle);
	const float s = sin(angle);
	return float3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

float3 EvaluateAmbient(SurfaceParams s)
{
	if (u_Pass.HasEnvironment == 0)
		return c_AmbientStrength * s.Albedo * s.AO;

	const float NdotV = max(dot(s.N, s.V), 1e-4);
	const float3 Nr = RotateY(s.N, u_Pass.EnvRotation);
	const float3 Rr = RotateY(reflect(-s.V, s.N), u_Pass.EnvRotation);

	const float3 F = F_SchlickRoughness(NdotV, s.F0, s.Roughness);
	const float3 kD = (1.0 - F) * (1.0 - s.Metallic);

	const float3 diffuseIBL = SampleBindlessCube(u_Pass.IrradianceIndex, SAMPLER_LINEAR_CLAMP, Nr).rgb * s.Albedo;
	const float3 prefiltered =
		SampleBindlessCubeLod(u_Pass.PrefilterIndex, SAMPLER_LINEAR_CLAMP, Rr, s.Roughness * u_Pass.MaxReflectionLod).rgb;

	const float2 dfg = EnvBRDFApprox(NdotV, s.Roughness);
	const float3 specularIBL = prefiltered * (s.F0 * dfg.x + dfg.y);

	const float specAO = SpecularOcclusion(NdotV, s.AO, s.Roughness);

	return (kD * diffuseIBL * s.AO + specularIBL * specAO) * u_Pass.EnvIntensity;
}
