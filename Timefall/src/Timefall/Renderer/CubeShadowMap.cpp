#include "tfpch.h"
#include "Timefall/Renderer/CubeShadowMap.h"

namespace Timefall
{
	namespace
	{
		class StubCubeShadowMap : public CubeShadowMap
		{
		public:
			StubCubeShadowMap(uint32_t resolution, uint32_t cubeCount)
				: m_Resolution(resolution),
				  m_CubeCount(cubeCount)
			{}

			void BeginRenderPass() override {}
			void BindFace(uint32_t, uint32_t) override {}
			void EndRenderPass() override {}

			void BindForSampling(uint32_t) const override {}

			uint32_t GetResolution() const override { return m_Resolution; }
			uint32_t GetCubeCount() const override { return m_CubeCount; }

		private:
			uint32_t m_Resolution;
			uint32_t m_CubeCount;
		};
	}

	Ref<CubeShadowMap> CubeShadowMap::Create(uint32_t resolution, uint32_t cubeCount)
	{
		return CreateRef<StubCubeShadowMap>(resolution, cubeCount);
	}
}
