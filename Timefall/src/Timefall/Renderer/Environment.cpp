#include "tfpch.h"
#include "Timefall/Renderer/Environment.h"

namespace Timefall
{
	namespace
	{
		class StubEnvironment : public Environment
		{
		public:
			StubEnvironment()
				: m_Skybox(TextureCube::Create(1)),
				  m_Irradiance(TextureCube::Create(1)),
				  m_Prefilter(TextureCube::Create(1))
			{}

			Ref<TextureCube> GetSkyboxMap() const override { return m_Skybox; }
			Ref<TextureCube> GetIrradianceMap() const override { return m_Irradiance; }
			Ref<TextureCube> GetPrefilterMap() const override { return m_Prefilter; }
			uint32_t GetPrefilterMipLevels() const override { return 1; }

		private:
			Ref<TextureCube> m_Skybox;
			Ref<TextureCube> m_Irradiance;
			Ref<TextureCube> m_Prefilter;
		};
	}

	Ref<Environment> Environment::Create(const Ref<Texture2D>&)
	{
		return CreateRef<StubEnvironment>();
	}
}
