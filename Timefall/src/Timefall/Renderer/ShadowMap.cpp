#include "tfpch.h"
#include "Timefall/Renderer/ShadowMap.h"

namespace Timefall
{
	namespace
	{
		class StubShadowMap : public ShadowMap
		{
		public:
			StubShadowMap(uint32_t resolution, uint32_t layers)
				: m_Resolution(resolution),
				  m_Layers(layers)
			{}

			void BeginRenderPass() override {}
			void BindLayer(uint32_t) override {}
			void EndRenderPass() override {}

			void BindForSampling(uint32_t) const override {}

			uint32_t GetDepthTextureID() const override { return 0; }
			uint32_t GetResolution() const override { return m_Resolution; }
			uint32_t GetLayerCount() const override { return m_Layers; }

		private:
			uint32_t m_Resolution;
			uint32_t m_Layers;
		};
	}

	Ref<ShadowMap> ShadowMap::Create(uint32_t resolution, uint32_t layers)
	{
		return CreateRef<StubShadowMap>(resolution, layers);
	}
}
