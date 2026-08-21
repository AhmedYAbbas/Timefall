//--------------------------
// - Timefall -
// HDR Resolve: exposure -> tonemap -> sRGB encode. Fullscreen triangle (no vertex buffer)
// --------------------------

import Common.Bindings;

struct ResolvePush
{
	uint HDRColorIndex;
	float ExposureEV;
	int Operator;
	float WhitePoint;
}

[[vk::push_constant]]
ConstantBuffer<ResolvePush> u_Push;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
}

[shader("vertex")]
VSOutput vsMain(uint vertexID : SV_VertexID)
{
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2); // (0,0),(2,0),(0,2)
    VSOutput output;
    output.uv = float2(pos.x, 1.0f - pos.y);
    output.position = float4(pos * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

uniform Sampler2D u_HDRColor;
uniform float u_ExposureEV;
uniform int u_Operator;
uniform float u_WhitePoint;


// Accurate sRGB OETF (linear -> display).
float3 LinearToSRGB(float3 c)
{
	bool3 cutoff = (c < float3(0.0031308));
	float3 low  = c * 12.92;
	float3 high = 1.055 * pow(c, float3(1.0 / 2.4)) - 0.055;
	return lerp(high, low, (float3)cutoff);
}

float3 Reinhard(float3 x)
{
	return x / (1.0 + x);
}

float3 ReinhardExtended(float3 x, float white)
{
	return (x * (1.0 + x / (white * white))) / (1.0 + x);
}

float3 Hable(float3 x)
{
	const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 HableFilmic(float3 c)
{
	const float W = 11.2;
	float3 curr = Hable(c * 2.0);
	float3 whiteScale = 1.0 / Hable(float3(W));
	return curr * whiteScale;
}

float3 ACESNarkowicz(float3 x)
{
	const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Stephen Hill fitted ACES (row-major M * v).
static const float3x3 ACESInputMat = float3x3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777);
static const float3x3 ACESOutputMat = float3x3(
	 1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602);

float3 RRTAndODTFit(float3 v)
{
	float3 a = v * (v + 0.0245786) - 0.000090537;
	float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}

float3 ACESHill(float3 color)
{
	color = mul(ACESInputMat, color);
	color = RRTAndODTFit(color);
	color = mul(ACESOutputMat, color);
	return clamp(color, 0.0, 1.0);
}

// Minimal AgX (Troy Sobotka / community GLSL port). Returns linear; the shared sRGB OETF encodes.
static const float3x3 AgXMat = float3x3(
	0.842479062253094,  0.0784335999999992, 0.0792237451477643,
	0.0423282422610123, 0.878468636469772,  0.0791661274605434,
	0.0423756549057051, 0.0784336, 0.879142973793104);
static const float3x3 AgXMatInv = float3x3(
	 1.19687900512017,   -0.0980208811401368, -0.0990297440797205,
	-0.0528968517574562,  1.15190312990417,   -0.0989611768448433,
	-0.0529716355144438, -0.0980434501171241,  1.15107367264116);

float3 AgXContrast(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x
		+ 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 AgX(float3 val)
{
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	val = mul(AgXMat, val);
	val = clamp(log2(max(val, 1e-10)), minEv, maxEv);
	val = (val - minEv) / (maxEv - minEv);
	val = AgXContrast(val);
	val = mul(AgXMatInv, val);
	return max(val, 0.0);
}

float3 KhronosPBRNeutral(float3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return lerp(color, newPeak * float3(1.0), g);
}

float3 Tonemap(float3 c)
{
	switch (u_Push.Operator)
	{
		case 0:  return c;                                // None / Linear
		case 1:  return Reinhard(c);
		case 2:  return ReinhardExtended(c, u_WhitePoint);
		case 3:  return HableFilmic(c);
		case 4:  return ACESNarkowicz(c);
		case 5:  return ACESHill(c);
		case 6:  return AgX(c);
		case 7:  return KhronosPBRNeutral(c);
		default: return c;
	}
}

[shader("fragment")]
float4 fsMain(VSOutput i) : SV_Target
{
    const float3 hdrColor = SampleBindless(u_Push.HDRColorIndex, SAMPLER_LINEAR_CLAMP, i.uv).rgb * exp2(u_Push.ExposureEV);
	return float4(LinearToSRGB(Tonemap(hdrColor)), 1.0f);
}
