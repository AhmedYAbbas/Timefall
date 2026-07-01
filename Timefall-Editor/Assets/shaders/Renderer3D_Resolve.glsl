//--------------------------
// - Timefall -
// HDR Resolve: exposure -> tonemap -> sRGB encode. Fullscreen triangle (no vertex buffer).
// --------------------------

#type vertex
#version 450 core

// Single oversized triangle covering the screen; UVs derived from clip position.
out vec2 v_TexCoord;

void main()
{
	vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); // (0,0),(2,0),(0,2)
	v_TexCoord = pos;
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450 core

in vec2 v_TexCoord;

uniform sampler2D u_HdrColor;
uniform float u_ExposureEV;   // exposure compensation in stops
uniform int   u_Operator;
uniform float u_WhitePoint;

layout(location = 0) out vec4 o_Color;

// Accurate sRGB OETF (linear -> display).
vec3 LinearToSRGB(vec3 c)
{
	bvec3 cutoff = lessThan(c, vec3(0.0031308));
	vec3 low  = c * 12.92;
	vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(high, low, cutoff);
}

vec3 Reinhard(vec3 x)
{
	return x / (1.0 + x);
}

vec3 ReinhardExtended(vec3 x, float white)
{
	return (x * (1.0 + x / (white * white))) / (1.0 + x);
}

vec3 Hable(vec3 x)
{
	const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 HableFilmic(vec3 c)
{
	const float W = 11.2;
	vec3 curr = Hable(c * 2.0);
	vec3 whiteScale = 1.0 / Hable(vec3(W));
	return curr * whiteScale;
}

vec3 ACESNarkowicz(vec3 x)
{
	const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Stephen Hill fitted ACES (matrices pre-transposed for GLSL column-major M * v).
const mat3 ACESInputMat = mat3(
	0.59719, 0.07600, 0.02840,
	0.35458, 0.90834, 0.13383,
	0.04823, 0.01566, 0.83777);
const mat3 ACESOutputMat = mat3(
	 1.60475, -0.10208, -0.00327,
	-0.53108,  1.10813, -0.07276,
	-0.07367, -0.00605,  1.07602);

vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}

vec3 ACESHill(vec3 color)
{
	color = ACESInputMat * color;
	color = RRTAndODTFit(color);
	color = ACESOutputMat * color;
	return clamp(color, 0.0, 1.0);
}

// Minimal AgX (Troy Sobotka / community GLSL port). Returns linear; the shared sRGB OETF encodes.
const mat3 AgXMat = mat3(
	0.842479062253094,  0.0423282422610123, 0.0423756549057051,
	0.0784335999999992, 0.878468636469772,  0.0784336,
	0.0792237451477643, 0.0791661274605434, 0.879142973793104);
const mat3 AgXMatInv = mat3(
	 1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
	-0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
	-0.0990297440797205, -0.0989611768448433,  1.15107367264116);

vec3 AgXContrast(vec3 x)
{
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x
		+ 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 AgX(vec3 val)
{
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	val = AgXMat * val;
	val = clamp(log2(max(val, 1e-10)), minEv, maxEv);
	val = (val - minEv) / (maxEv - minEv);
	val = AgXContrast(val);
	val = AgXMatInv * val;
	return max(val, 0.0);
}

vec3 KhronosPBRNeutral(vec3 color)
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
	return mix(color, newPeak * vec3(1.0), g);
}

vec3 Tonemap(vec3 c)
{
	switch (u_Operator)
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

void main()
{
	vec3 hdr = texture(u_HdrColor, v_TexCoord).rgb;
	hdr *= exp2(u_ExposureEV);
	vec3 mapped = Tonemap(hdr);
	o_Color = vec4(LinearToSRGB(mapped), 1.0);
}
