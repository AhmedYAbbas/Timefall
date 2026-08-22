//--------------------------
// - Timefall -
// Shared BRDF math. The analytic path (Lit) and the image-based path (IBL) both evaluate it
// --------------------------

import Common.Bindings;

static const float PI = 3.14159265359;

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

// GGX / Trowbridge-Reitz normal distribution.
float D_GGX(float NdotH, float a)
{
	const float a2 = a * a;
	const float d = (NdotH * a2 - NdotH) * NdotH + 1.0;
	return a2 / (PI * d * d);
}

// Height-correlated Smith visibility - already folds in the 1/(4*NdotV*NdotL) denominator.
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a)
{
	const float a2 = a * a;
	const float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	const float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	return 0.5 / max(ggxV + ggxL, 1e-5);
}

float3 F_Schlick(float u, float3 f0)
{
	const float f = pow(1.0 - u, 5.0);
	return f0 + (1.0 - f0) * f;
}

// Roughness-aware Fresnel for ambient (Lagarde).
float3 F_SchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Karis analytic environment-BRDF fit (no LUT). Returns (scale, bias); the single-scatter
// directional albedo for white F0 is (scale + bias).
float2 EnvBRDFApprox(float NdotV, float roughness)
{
	const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
	const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
	const float4 r = roughness * c0 + c1;
	const float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
	return float2(-1.04, 1.04) * a004 + r.zw;
}

// Horizon-based specular occlusion from diffuse AO (Lagarde, "Moving Frostbite to PBR").
float SpecularOcclusion(float NdotV, float ao, float roughness)
{
	return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// Improved geometric specular AA (Kaplanyan 2016): bump roughness by the sub-pixel normal
// variance estimated from screen-space derivatives of the shading normal.
float FilterRoughness(float3 N, float roughness)
{
	const float SIGMA2 = 0.25;
	const float KAPPA = 0.18;
	const float3 dndu = ddx(N);
	const float3 dndv = ddy(N);
	const float variance = SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
	const float a = roughness * roughness;
	const float a2 = clamp(a * a + min(2.0 * variance, KAPPA), 0.0, 1.0);
	return sqrt(sqrt(a2)); // back to perceptual roughness
}

// Cook-Torrance contribution of one light. radiance folds in light colour, intensity,
// attenuation/cone and shadow (point/spot); the directional caller applies its shadow outside.
float3 CookTorrance(SurfaceParams s, float3 L, float3 radiance)
{
	const float NdotL = max(dot(s.N, L), 0.0);
	if (NdotL <= 0.0)
		return float3(0.0);

	const float3 H = normalize(s.V + L);
	const float NdotV = max(dot(s.N, s.V), 1e-4);
	const float NdotH = max(dot(s.N, H), 0.0);
	const float LdotH = max(dot(L, H), 0.0);
	const float a = s.Roughness * s.Roughness;

	const float D = D_GGX(NdotH, a);
	const float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a);
	const float3 F = F_Schlick(LdotH, s.F0);

	float3 specular = D * Vis * F; // Vis already carries 1/(4*NdotV*NdotL)

	// Kulla-Conty multi-scatter energy compensation (rough metals keep their energy).
	const float2 dfg = EnvBRDFApprox(NdotV, s.Roughness);
	const float Ess = dfg.x + dfg.y;
	specular *= 1.0 + s.F0 * (1.0 / max(Ess, 1e-4) - 1.0);

	const float3 kD = (float3(1.0) - F) * (1.0 - s.Metallic);
	const float3 diffuse = kD * s.Albedo * (1.0 / PI);

	return (diffuse + specular) * radiance * NdotL;
}

// Physical inverse-square falloff with Karis range windowing (smooth cutoff to 0 at range).
float Attenuate(float dist, float range)
{
	const float d2 = dist * dist;
	const float factor = d2 / (range * range); // (d/r)^2
	const float window = clamp(1.0 - factor * factor, 0.0, 1.0); // 1 - (d/r)^4
	return (window * window) / max(d2, 1e-4); // 1/d^2 * window^2
}
