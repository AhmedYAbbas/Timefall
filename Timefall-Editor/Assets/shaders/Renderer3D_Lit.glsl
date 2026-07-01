//--------------------------
// - Timefall -
// Renderer3D Lit Shader (Phase 9.2 — directional/point/spot Blinn-Phong; material hardcoded)
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	mat4 u_View;
	vec3 u_CameraPosition;
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out mat3 v_TBN;   // occupies locations 2,3,4

void main()
{
	vec4 world = u_Model * vec4(a_Position, 1.0);
	v_WorldPos = world.xyz;

	vec3 N = normalize(u_NormalMatrix * a_Normal);
	vec3 T = normalize(u_NormalMatrix * a_Tangent);
	T = normalize(T - N * dot(N, T));                  // Gram-Schmidt re-orthogonalize
	vec3 Bw = u_NormalMatrix * a_Bitangent;
	vec3 B = cross(N, T) * sign(dot(cross(N, T), Bw)); // preserve UV handedness

	v_TBN = mat3(T, B, N);
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * world;
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in mat3 v_TBN;   // occupies locations 2,3,4

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	mat4 u_View;
	vec3 u_CameraPosition;
};

struct DirLight   { vec4 Direction; vec4 Color; };                          // Color.a = intensity
struct PointLight { vec4 Position;  vec4 Color; };                          // Position.w = range, Color.a = intensity
struct SpotLight  { vec4 Position;  vec4 Direction; vec4 Color; vec4 Params; };
// Spot: Position.xyz = position, Direction.xyz = direction, Color.rgb = color,
//       Params = (range, innerCos, outerCos, intensity)

const int MAX_DIR   = 4;
const int MAX_POINT = 32;
const int MAX_SPOT  = 16;

layout(std140, binding = 1) uniform Lights
{
	DirLight   u_DirLights[MAX_DIR];
	PointLight u_PointLights[MAX_POINT];
	SpotLight  u_SpotLights[MAX_SPOT];
	int u_DirCount;
	int u_PointCount;
	int u_SpotCount;
};

const int MAX_CASCADES = 4;

layout(std140, binding = 2) uniform Shadows
{
	mat4 u_LightViewProj[MAX_CASCADES];
	vec4 u_CascadeSplits;
	vec4 u_CascadeTexelWorld;
	vec4 u_CascadeDepthRange;
	int   u_CascadeCount;
	int   u_VisualizeCascades;
	float u_LightSize;
	float u_DepthBias;
	float u_CascadeBlend;
	int   u_BlockerSamples;
	int   u_PCFSamples;
	int   u_SoftShadows;
};

layout(std140, binding = 3) uniform SpotShadows
{
	mat4 u_SpotLightViewProj[MAX_SPOT];
	vec4 u_SpotShadowParams[MAX_SPOT];   // x = casts, y = lightSize, z = depthBias
};

layout(std140, binding = 4) uniform PointShadows
{
	vec4 u_PointShadowParams[MAX_POINT]; // x = casts, y = lightSize, z = depthBias, w = cubeLayer
};

uniform int u_EntityID;

uniform vec3  u_BaseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_NormalStrength;
uniform vec3  u_Emissive;
uniform float u_EmissiveIntensity;
uniform sampler2D u_BaseColorMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;
uniform sampler2D u_NormalMap;
uniform sampler2DArray u_ShadowMap;
uniform sampler2DArray u_SpotShadowMap;
uniform samplerCubeArray u_PointShadowMap;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

const float c_AmbientStrength = 0.09f;

const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution.
float D_GGX(float NdotH, float a)
{
	float a2 = a * a;
	float d  = (NdotH * a2 - NdotH) * NdotH + 1.0;
	return a2 / (PI * d * d);
}

// Height-correlated Smith visibility — already folds in the 1/(4·NdotV·NdotL) denominator.
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a)
{
	float a2   = a * a;
	float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	return 0.5 / max(ggxV + ggxL, 1e-5);
}

vec3 F_Schlick(float u, vec3 f0)
{
	float f = pow(1.0 - u, 5.0);
	return f0 + (1.0 - f0) * f;
}

// Karis analytic environment-BRDF fit (no LUT). Returns (scale, bias); the single-scatter
// directional albedo for white F0 is (scale + bias).
vec2 EnvBRDFApprox(float NdotV, float roughness)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572,  0.022);
	const vec4 c1 = vec4( 1.0,  0.0425,  1.04,  -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
	return vec2(-1.04, 1.04) * a004 + r.zw;
}

// Improved geometric specular AA (Kaplanyan 2016): bump roughness by the sub-pixel
// normal variance estimated from screen-space derivatives of the shading normal.
float FilterRoughness(vec3 N, float roughness)
{
	const float SIGMA2 = 0.25;
	const float KAPPA  = 0.18;
	vec3  dndu = dFdx(N);
	vec3  dndv = dFdy(N);
	float variance = SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
	float a  = roughness * roughness;
	float a2 = clamp(a * a + min(2.0 * variance, KAPPA), 0.0, 1.0);
	return sqrt(sqrt(a2));   // back to perceptual roughness
}

// Cook-Torrance contribution of one light. radiance folds in light color, intensity,
// attenuation/cone, and shadow (point/spot); the directional caller applies its shadow outside.
vec3 CookTorrance(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0)
{
	float NdotL = max(dot(N, L), 0.0);
	if (NdotL <= 0.0)
		return vec3(0.0);

	vec3  H     = normalize(V + L);
	float NdotV = max(dot(N, V), 1e-4);
	float NdotH = max(dot(N, H), 0.0);
	float LdotH = max(dot(L, H), 0.0);
	float a     = roughness * roughness;

	float D   = D_GGX(NdotH, a);
	float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a);
	vec3  F   = F_Schlick(LdotH, F0);

	vec3 specular = D * Vis * F;                       // V already carries 1/(4·NdotV·NdotL)

	// Kulla-Conty multi-scatter energy compensation (rough metals keep their energy).
	vec2  dfg = EnvBRDFApprox(NdotV, roughness);
	float Ess = dfg.x + dfg.y;
	specular *= 1.0 + F0 * (1.0 / max(Ess, 1e-4) - 1.0);

	vec3 kD       = (vec3(1.0) - F) * (1.0 - metallic);
	vec3 diffuse  = kD * albedo * (1.0 / PI);

	return (diffuse + specular) * radiance * NdotL;
}

// Range-based attenuation: smoothly 1 at the light, 0 at distance == range.
float Attenuate(float dist, float range)
{
	float x = clamp(1.0 - (dist * dist) / (range * range), 0.0, 1.0);
	return x * x;
}

const float BLOCKER_SEARCH_TEXELS = 24.0;
const float MAX_FILTER_TEXELS     = 24.0;
const float FIXED_PCF_TEXELS      = 2.0;  // fixed-kernel radius when soft shadows are off

const float GOLDEN_ANGLE    = 2.39996323;

// Interleaved gradient noise (Jimenez 2014): low-discrepancy per-pixel rotation seed.
float InterleavedGradientNoise(vec2 p)
{
	return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// Evenly-distributed disk sample (Vogel/sunflower), rotated per pixel.
vec2 VogelDisk(int i, int count, float rotation)
{
	float r = sqrt((float(i) + 0.5) / float(count));
	float theta = float(i) * GOLDEN_ANGLE + rotation;
	return r * vec2(cos(theta), sin(theta));
}

int SelectCascade(float viewDepth)
{
	for (int c = 0; c < u_CascadeCount; ++c)
		if (viewDepth < u_CascadeSplits[c])
			return c;
	return u_CascadeCount - 1;
}

// PCSS for one cascade layer: blocker search -> penumbra estimate -> variable-kernel PCF.
float SampleShadowCascade(vec3 worldPos, int cascade, vec3 N, vec3 L)
{
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	vec3 offsetPos = worldPos + N * (u_CascadeTexelWorld[cascade] * 2.0 * (1.0 - NdotL));

	vec4 lightSpace = u_LightViewProj[cascade] * vec4(offsetPos, 1.0);
	vec3 proj = lightSpace.xyz / lightSpace.w;
	proj = proj * 0.5 + 0.5;
	if (proj.z > 1.0)
		return 0.0;

	float bias = max(0.0025 * (1.0 - NdotL), 0.0005) * u_DepthBias;
	float texelUV = 1.0 / float(textureSize(u_ShadowMap, 0).x);
	float receiver = proj.z;

	float rotation = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;

	float filterUV;
	if (u_SoftShadows != 0)
	{
		// 1. Blocker search: average depth of texels closer to the light than the receiver.
		// Fixed (un-rotated) pattern so the penumbra SIZE stays spatially coherent; per-pixel
		// rotation here would make the kernel size flicker, which no amount of PCF taps can fix.
		float searchUV = BLOCKER_SEARCH_TEXELS * texelUV;
		float blockerSum = 0.0;
		int   blockerCount = 0;
		for (int i = 0; i < u_BlockerSamples; ++i)
		{
			vec2 off = VogelDisk(i, u_BlockerSamples, 0.0) * searchUV;
			float d = texture(u_ShadowMap, vec3(proj.xy + off, float(cascade))).r;
			if (d < receiver - bias)
			{
				blockerSum += d;
				blockerCount++;
			}
		}
		if (blockerCount == 0)
			return 0.0; // no occluder -> fully lit

		float avgBlocker = blockerSum / float(blockerCount);

		// 2. Penumbra (directional): depth gap -> world, scale by light size, back to texels.
		float worldGap = (receiver - avgBlocker) * u_CascadeDepthRange[cascade];
		float penumbraTexels = (worldGap * u_LightSize) / u_CascadeTexelWorld[cascade];
		filterUV = clamp(penumbraTexels, 1.0, MAX_FILTER_TEXELS) * texelUV;
	}
	else
	{
		// Fixed-kernel PCF: uniform softness, no blocker search / penumbra estimate.
		filterUV = FIXED_PCF_TEXELS * texelUV;
	}

	// 3. Variable-kernel PCF over the rotated Vogel disk.
	float sum = 0.0;
	for (int i = 0; i < u_PCFSamples; ++i)
	{
		vec2 off = VogelDisk(i, u_PCFSamples, rotation) * filterUV;
		float closest = texture(u_ShadowMap, vec3(proj.xy + off, float(cascade))).r;
		sum += (receiver - bias) > closest ? 1.0 : 0.0;
	}

	return sum / float(u_PCFSamples);
}

// Sun shadow with a blend across the cascade boundary to hide the resolution seam.
float ComputeSunShadow(vec3 worldPos, float viewDepth, int cascade, vec3 N, vec3 L)
{
	float shadow = SampleShadowCascade(worldPos, cascade, N, L);

	if (cascade + 1 < u_CascadeCount)
	{
		float splitFar  = u_CascadeSplits[cascade];
		float prevSplit = cascade == 0 ? 0.0 : u_CascadeSplits[cascade - 1];
		float band = (splitFar - prevSplit) * u_CascadeBlend;
		float t = (viewDepth - (splitFar - band)) / max(band, 1e-4);
		if (t > 0.0)
		{
			float next = SampleShadowCascade(worldPos, cascade + 1, N, L);
			shadow = mix(shadow, next, clamp(t, 0.0, 1.0));
		}
	}

	return shadow;
}

float LinearizeDepth(float d, float near, float far)
{
	float z = d * 2.0 - 1.0; // depth-buffer [0,1] -> NDC [-1,1]
	return (2.0 * near * far) / (far + near - z * (far - near));
}

float SampleSpotShadow(vec3 worldPos, int i, vec3 N, vec3 L)
{
	vec4 ls = u_SpotLightViewProj[i] * vec4(worldPos, 1.0);
	vec3 proj = ls.xyz / ls.w;
	proj = proj * 0.5 + 0.5;
	if (proj.z > 1.0)
		return 0.0;

	float NdotL     = clamp(dot(N, L), 0.0, 1.0);
	float bias      = max(0.0025 * (1.0 - NdotL), 0.0005) * u_SpotShadowParams[i].z;
	float lightSize = u_SpotShadowParams[i].y;
	float receiver  = proj.z;
	float texelUV   = 1.0 / float(textureSize(u_SpotShadowMap, 0).x);
	float rotation  = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;

	float filterUV;
	if (u_SoftShadows != 0)
	{
		float searchUV = BLOCKER_SEARCH_TEXELS * texelUV;
		float blockerSum = 0.0;
		int   blockerCount = 0;
		for (int s = 0; s < u_BlockerSamples; ++s)
		{
			vec2 off = VogelDisk(s, u_BlockerSamples, 0.0) * searchUV;
			float d = texture(u_SpotShadowMap, vec3(proj.xy + off, float(i))).r;
			if (d < receiver - bias)
			{
				blockerSum += d;
				blockerCount++;
			}
		}
		if (blockerCount == 0)
			return 0.0;

		float avgBlocker = blockerSum / float(blockerCount);

		// Fernando PCSS in linear depth: world penumbra width -> UV via the frustum width at the receiver.
		float near = 0.05;
		float far  = u_SpotLights[i].Params.x;                              // spot range
		float outerCos = u_SpotLights[i].Params.z;
		float tanHalf  = sqrt(max(1.0 - outerCos * outerCos, 0.0)) / max(outerCos, 1e-3);

		float linReceiver = LinearizeDepth(receiver, near, far);
		float linBlocker  = LinearizeDepth(avgBlocker, near, far);
		float worldPenumbra = (linReceiver - linBlocker) / linBlocker * lightSize;
		float worldPerUV    = 2.0 * linReceiver * tanHalf;                  // frustum width at the receiver
		filterUV = clamp(worldPenumbra / max(worldPerUV, 1e-4), texelUV, MAX_FILTER_TEXELS * texelUV);
	}
	else
	{
		filterUV = FIXED_PCF_TEXELS * texelUV;
	}

	float shadow = 0.0;
	for (int s = 0; s < u_PCFSamples; ++s)
	{
		vec2 off = VogelDisk(s, u_PCFSamples, rotation) * filterUV;
		float closest = texture(u_SpotShadowMap, vec3(proj.xy + off, float(i))).r;
		shadow += (receiver - bias) > closest ? 1.0 : 0.0;
	}
	return shadow / float(u_PCFSamples);
}

float SamplePointShadow(vec3 worldPos, int i, vec3 N, vec3 L)
{
	int   layer = int(u_PointShadowParams[i].w);
	vec3  lightPos  = u_PointLights[i].Position.xyz;
	float far       = u_PointLights[i].Position.w;
	float lightSize = u_PointShadowParams[i].y;

	vec3  toFrag = worldPos - lightPos;
	float dR = length(toFrag);
	vec3  dir = toFrag / max(dR, 1e-4);

	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	// Larger world-distance bias than sun/spot: the cube's linear-distance depth is coarser.
	float bias = max(0.25 * (1.0 - NdotL), 0.05) * u_PointShadowParams[i].z;

	// Tangent basis perpendicular to the light->fragment direction.
	vec3 up = abs(dir.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	vec3 T  = normalize(cross(up, dir));
	vec3 B  = cross(dir, T);
	float ign = InterleavedGradientNoise(gl_FragCoord.xy);
	float rotation = ign * 6.2831853;

	float angularRadius;
	if (u_SoftShadows != 0)
	{
		float searchAngle = 0.05;
		float blockerSum = 0.0;
		int   blockerCount = 0;
		for (int k = 0; k < u_BlockerSamples; ++k)
		{
			vec2 o = VogelDisk(k, u_BlockerSamples, 0.0) * searchAngle;
			vec3 sd = normalize(dir + T * o.x + B * o.y);
			float st = texture(u_PointShadowMap, vec4(sd, float(layer))).r * far;
			if (st < dR - bias)
			{
				blockerSum += st;
				blockerCount++;
			}
		}
		if (blockerCount == 0)
			return 0.0;

		float dBlk = blockerSum / float(blockerCount);
		float worldPenumbra = (dR - dBlk) / dBlk * lightSize;
		angularRadius = clamp(worldPenumbra / dR, 0.0015, 0.06);
	}
	else
	{
		angularRadius = 0.01;
	}

	float shadow = 0.0;
	for (int k = 0; k < u_PCFSamples; ++k)
	{
		// Per-pixel radial jitter breaks the Vogel disk's fixed-radius rings into grain.
		float jitter = 0.7 + 0.3 * fract(ign + float(k) * 0.61803399);
		vec2 o = VogelDisk(k, u_PCFSamples, rotation) * angularRadius * jitter;
		vec3 sd = normalize(dir + T * o.x + B * o.y);
		float st = texture(u_PointShadowMap, vec4(sd, float(layer))).r * far;
		shadow += (dR - bias) > st ? 1.0 : 0.0;
	}
	return shadow / float(u_PCFSamples);
}

void main()
{
	vec3 mapN = texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0;
	mapN.xy *= u_NormalStrength;
	vec3 N = normalize(v_TBN * normalize(mapN));
	vec3 V = normalize(u_CameraPosition - v_WorldPos);

	float viewDepth = -(u_View * vec4(v_WorldPos, 1.0)).z;
	int cascade = u_CascadeCount > 0 ? SelectCascade(viewDepth) : 0;

	vec3  albedo    = u_BaseColor * texture(u_BaseColorMap, v_TexCoord).rgb;
	float metallic  = u_Metallic  * texture(u_MetallicMap,  v_TexCoord).r;
	float roughness = clamp(u_Roughness * texture(u_RoughnessMap, v_TexCoord).r, 0.045, 1.0);
	roughness = clamp(FilterRoughness(N, roughness), 0.045, 1.0);
	float ao        = texture(u_AOMap, v_TexCoord).r;

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// Flat ambient placeholder (no IBL this pass). Metals read dark here — expected.
	vec3 Lo = c_AmbientStrength * albedo * ao;

	// Directional
	for (int i = 0; i < u_DirCount; i++)
	{
		vec3 L = normalize(-u_DirLights[i].Direction.xyz);
		vec3 radiance = u_DirLights[i].Color.rgb * u_DirLights[i].Color.a;

		float shadow = (i == 0 && u_CascadeCount > 0) ? ComputeSunShadow(v_WorldPos, viewDepth, cascade, N, L) : 0.0;
		Lo += (1.0 - shadow) * CookTorrance(N, V, L, radiance, albedo, metallic, roughness, F0);
	}

	// Point
	for (int i = 0; i < u_PointCount; i++)
	{
		vec3 toLight = u_PointLights[i].Position.xyz - v_WorldPos;
		float dist = length(toLight);
		vec3 L = toLight / max(dist, 0.0001);
		float att = Attenuate(dist, u_PointLights[i].Position.w);
		vec3 radiance = u_PointLights[i].Color.rgb * u_PointLights[i].Color.a * att;
		if (u_PointShadowParams[i].x > 0.5)
			radiance *= (1.0 - SamplePointShadow(v_WorldPos, i, N, L));
		Lo += CookTorrance(N, V, L, radiance, albedo, metallic, roughness, F0);
	}

	// Spot
	for (int i = 0; i < u_SpotCount; i++)
	{
		float range     = u_SpotLights[i].Params.x;
		float innerCos  = u_SpotLights[i].Params.y;
		float outerCos  = u_SpotLights[i].Params.z;
		float intensity = u_SpotLights[i].Params.w;

		vec3 toLight = u_SpotLights[i].Position.xyz - v_WorldPos;
		float dist = length(toLight);
		vec3 L = toLight / max(dist, 0.0001);
		float att = Attenuate(dist, range);

		vec3  spotDir = normalize(u_SpotLights[i].Direction.xyz);
		float theta   = dot(-L, spotDir);                       // 1 when fragment is on the cone axis
		float cone    = clamp((theta - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);

		vec3 radiance = u_SpotLights[i].Color.rgb * intensity * att * cone;
		if (u_SpotShadowParams[i].x > 0.5)
			radiance *= (1.0 - SampleSpotShadow(v_WorldPos, i, N, L));
		Lo += CookTorrance(N, V, L, radiance, albedo, metallic, roughness, F0);
	}

	Lo += u_Emissive * u_EmissiveIntensity * texture(u_EmissiveMap, v_TexCoord).rgb;

	if (u_VisualizeCascades == 1)
	{
		vec3 tints[4] = vec3[4](vec3(1.0, 0.4, 0.4), vec3(0.4, 1.0, 0.4), vec3(0.4, 0.4, 1.0), vec3(1.0, 1.0, 0.4));
		Lo *= tints[cascade];
	}

	o_Color = vec4(Lo, 1.0);
	o_EntityID = u_EntityID;
}
