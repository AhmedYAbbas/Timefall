//--------------------------
// - Timefall -
// Thin-film interference. Showcase effect: rewrites the PBR surface inputs, then hands
// the surface back to the normal Cook-Torrance + IBL path untouched.
// --------------------------

import Common.Bindings;
import Common.BRDF;

// ---- gradient noise --------------------------------------------------------
// Gradient, not value: value noise pins its extrema to the integer lattice, which reads as
// axis-aligned blocks on axis-aligned geometry. Gradient noise is zero at every lattice point,
// so the cell structure disappears.

float3 Hash33(float3 p)
{
	p = float3(dot(p, float3(127.1, 311.7, 74.7)), dot(p, float3(269.5, 183.3, 246.1)), dot(p, float3(113.5, 271.9, 124.6)));
	return -1.0 + 2.0 * frac(sin(p) * 43758.5453123);
}

float GradientNoise(float3 p)
{
	const float3 i = floor(p);
	const float3 f = frac(p);
	const float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0); // quintic: C2, so no derivative seams

	return lerp(lerp(lerp(dot(Hash33(i + float3(0, 0, 0)), f - float3(0, 0, 0)),
						  dot(Hash33(i + float3(1, 0, 0)), f - float3(1, 0, 0)), u.x),
					 lerp(dot(Hash33(i + float3(0, 1, 0)), f - float3(0, 1, 0)),
						  dot(Hash33(i + float3(1, 1, 0)), f - float3(1, 1, 0)), u.x), u.y),
				lerp(lerp(dot(Hash33(i + float3(0, 0, 1)), f - float3(0, 0, 1)),
						  dot(Hash33(i + float3(1, 0, 1)), f - float3(1, 0, 1)), u.x),
					 lerp(dot(Hash33(i + float3(0, 1, 1)), f - float3(0, 1, 1)),
						  dot(Hash33(i + float3(1, 1, 1)), f - float3(1, 1, 1)), u.x), u.y), u.z);
}

// Returns [0,1]. Octaves are rotated, not just scaled: scaling alone keeps every octave on the
// same axes so their lattices reinforce instead of cancelling.
float Fbm(float3 p, int octaves)
{
	const float3x3 rot = float3x3(0.00, 0.80, 0.60, -0.80, 0.36, -0.48, -0.60, -0.48, 0.64);

	float sum = 0.0;
	float amp = 0.5;
	float norm = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		sum += amp * GradientNoise(p);
		norm += amp;
		p = mul(rot, p) * 2.02;
		amp *= 0.5;
	}
	return 0.5 + 0.5 * (sum / max(norm, 1e-5));
}

// ---- thin-film interference -----------------------------------------------

float3 ThinFilm(float cosThetaI, float d, float nFilm)
{
	const float sin2ThetaT = (1.0 - cosThetaI * cosThetaI) / (nFilm * nFilm);
	if (sin2ThetaT >= 1.0)
		return float3(1.0); // total internal reflection

	const float cosThetaT = sqrt(1.0 - sin2ThetaT);
	const float opd = 2.0 * nFilm * d * cosThetaT;

	const float3 lambda = float3(650.0, 550.0, 450.0); // nm, approx. R/G/B response
	const float3 phase = 2.0 * PI * opd / lambda + PI; // +PI: hard reflection flips the top interface

	return 0.5 + 0.5 * cos(phase);
}

// Schlick, scalar - how much light the surface reflects at this angle at all.
float FilmFresnel(float cosTheta, float f0)
{
	return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// ---- the showcase effect --------------------------------------------------

struct IridescenceParams
{
	float ThicknessMin; // nm
	float ThicknessMax; // nm
	float FilmIOR;
	float SwirlScale;
	float FlowSpeed;
	float Blend; // 0 = untouched PBR, 1 = full film
	float RoughnessMin;
	float RoughnessMax;
	int Octaves;
}

void ApplyIridescence(inout SurfaceParams s, float3 worldPos, float time, IridescenceParams p)
{
	const float3 flow = float3(0.0, 1.0, 0.35) * time * p.FlowSpeed;
	// smoothstep, not saturate(x * gain): it expands contrast with zero derivative at both ends,
	// so the range limits fade out instead of clipping to flat plateaus with hard borders
	const float swirl = smoothstep(0.25, 0.75, Fbm(worldPos * p.SwirlScale + flow, p.Octaves));

	const float cosThetaI = saturate(dot(s.N, s.V));
	float d = lerp(p.ThicknessMin, p.ThicknessMax, swirl);
	//d = lerp(p.ThicknessMin, p.ThicknessMax, pow(1.0 - cosThetaI, 1.5));
	// d = lerp(p.ThicknessMin, p.ThicknessMax, frac(worldPos.y * 0.35 + time * 0.1));

	float3 film = ThinFilm(cosThetaI, d, p.FilmIOR) * FilmFresnel(cosThetaI, 0.22);
	film = ThinFilm(cosThetaI, d, p.FilmIOR) * ThinFilm(cosThetaI, d * 0.37, 1.5f);

	s.Albedo = lerp(s.Albedo, film, p.Blend);
	s.Metallic = lerp(s.Metallic, 1.0, p.Blend);
	s.Roughness = clamp(lerp(s.Roughness, lerp(p.RoughnessMin, p.RoughnessMax, swirl), p.Blend), 0.045, 1.0);
	s.F0 = lerp(float3(0.04), s.Albedo, s.Metallic);
}
