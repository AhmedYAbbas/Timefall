//--------------------------
// - Timefall -
// Renderer3D Lit Shader (Phase 9.2 — directional/point/spot Blinn-Phong; material hardcoded)
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	mat4 u_View;
	vec3 u_CameraPosition;
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;

void main()
{
	vec4 world = u_Model * vec4(a_Position, 1.0);
	v_WorldPos = world.xyz;
	v_Normal = u_NormalMatrix * a_Normal;
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * world;
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;

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

uniform int u_EntityID;

uniform vec3  u_DiffuseColor;
uniform vec3  u_SpecularColor;
uniform float u_Shininess;
uniform sampler2D u_DiffuseMap;
uniform sampler2D u_SpecularMap;
uniform sampler2DArray u_ShadowMap;
uniform sampler2DArray u_SpotShadowMap;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

const float c_AmbientStrength = 0.09f;

// Blinn-Phong contribution of one light. L = surface->light (unit). radiance already folds in
// the light color, intensity, and any attenuation/cone factor.
vec3 BlinnPhong(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 matDiffuse, vec3 matSpecular, float shininess)
{
	float diff = max(dot(N, L), 0.0);
	vec3  H    = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), shininess);
	return (diff * matDiffuse + spec * matSpecular) * radiance;
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

float SampleSpotShadow(vec3 worldPos, int i, vec3 N, vec3 L)
{
	vec4 ls = u_SpotLightViewProj[i] * vec4(worldPos, 1.0);
	vec3 proj = ls.xyz / ls.w;
	proj = proj * 0.5 + 0.5;
	if (proj.z > 1.0)
		return 0.0;

	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	float bias = max(0.0025 * (1.0 - NdotL), 0.0005) * u_SpotShadowParams[i].z;
	float closest = texture(u_SpotShadowMap, vec3(proj.xy, float(i))).r;
	return (proj.z - bias) > closest ? 1.0 : 0.0;
}

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_CameraPosition - v_WorldPos);

	float viewDepth = -(u_View * vec4(v_WorldPos, 1.0)).z;
	int cascade = u_CascadeCount > 0 ? SelectCascade(viewDepth) : 0;

	vec3 matDiffuse  = u_DiffuseColor  * texture(u_DiffuseMap,  v_TexCoord).rgb;
	vec3 matSpecular = u_SpecularColor * texture(u_SpecularMap, v_TexCoord).rgb;
	//vec3 matSpecular = vec3(0.4f);

	vec3 color = c_AmbientStrength * matDiffuse;

	// Directional
	for (int i = 0; i < u_DirCount; i++)
	{
		vec3 L = normalize(-u_DirLights[i].Direction.xyz);
		vec3 radiance = u_DirLights[i].Color.rgb * u_DirLights[i].Color.a;

		float shadow = (i == 0 && u_CascadeCount > 0) ? ComputeSunShadow(v_WorldPos, viewDepth, cascade, N, L) : 0.0;
		color += (1.0 - shadow) * BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
	}

	// Point
	for (int i = 0; i < u_PointCount; i++)
	{
		vec3 toLight = u_PointLights[i].Position.xyz - v_WorldPos;
		float dist = length(toLight);
		vec3 L = toLight / max(dist, 0.0001);
		float att = Attenuate(dist, u_PointLights[i].Position.w);
		vec3 radiance = u_PointLights[i].Color.rgb * u_PointLights[i].Color.a * att;
		color += BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
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
		color += BlinnPhong(N, V, L, radiance, matDiffuse, matSpecular, u_Shininess);
	}

	if (u_VisualizeCascades == 1)
	{
		vec3 tints[4] = vec3[4](vec3(1.0, 0.4, 0.4), vec3(0.4, 1.0, 0.4), vec3(0.4, 0.4, 1.0), vec3(1.0, 1.0, 0.4));
		color *= tints[cascade];
	}

	o_Color = vec4(color, 1.0);
	o_Color.rgb = pow(o_Color.rgb, vec3(1.0/2.2)); // Gamma correction
	o_EntityID = u_EntityID;
}
