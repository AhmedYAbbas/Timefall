//--------------------------
// - Timefall -
// Renderer2D Quad Shader
// --------------------------

#type vertex
#version 330 core
		
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in int a_EntityID;
		
uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
flat out float v_TexIndex;
out float v_TilingFactor;
flat out int v_EntityID;
			
void main()
{
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment
#version 330 core
		
in vec4 v_Color;
in vec2 v_TexCoord;
flat in float v_TexIndex;
in float v_TilingFactor;
flat in int v_EntityID;

uniform sampler2D u_Textures[32];

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

void main()
{
	vec4 textureColor = texture(u_Textures[int(v_TexIndex)], v_TexCoord * v_TilingFactor);
	if (textureColor.a == 0.0)
		discard;

	o_Color = textureColor * v_Color;
	o_EntityID = v_EntityID;
}