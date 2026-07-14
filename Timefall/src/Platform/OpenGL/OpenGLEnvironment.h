#pragma once

#include "Timefall/Renderer/Environment.h"
#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/Mesh.h"

namespace Timefall
{
	class OpenGLEnvironment : public Environment
	{
	public:
		explicit OpenGLEnvironment(const Ref<Texture2D>& equirect);
		virtual ~OpenGLEnvironment();

		virtual Ref<TextureCube> GetSkyboxMap() const override { return m_Skybox; }
		virtual Ref<TextureCube> GetIrradianceMap() const override { return m_Irradiance; }
		virtual Ref<TextureCube> GetPrefilterMap() const override { return m_Prefilter; }
		virtual uint32_t GetPrefilterMipLevels() const override { return m_PrefilterMips; }

	private:
		Ref<TextureCube> m_Skybox;
		Ref<TextureCube> m_Irradiance;
		Ref<TextureCube> m_Prefilter;
		uint32_t m_PrefilterMips = 5;

		Ref<MeshSource> m_CubeMesh; // unit cube; positions double as sample directions
		uint32_t m_CaptureFBO = 0;
	};
}
