#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
uniform mat4 u_ViewProjection;
layout(location = 0) out vec3 v_LocalPos;

void main()
{
	v_LocalPos = a_Position;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_LocalPos;
layout(location = 0) out vec4 o_Color;

uniform sampler2D u_Equirect;

const vec2 invAtan = vec2(0.1591, 0.3183); // 1/(2pi), 1/pi
const float MAX_RADIANCE = 1.0e6; // safety ceiling against pathological source texels

vec2 SampleSphericalMap(vec3 dir)
{
	vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main()
{
	vec3 dir = normalize(v_LocalPos);
	vec3 color = texture(u_Equirect, SampleSphericalMap(dir)).rgb;
	o_Color = vec4(clamp(color, vec3(0.0), vec3(MAX_RADIANCE)), 1.0);
}
