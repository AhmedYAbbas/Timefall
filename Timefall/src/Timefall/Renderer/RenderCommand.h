#pragma once

#include "Timefall/Renderer/RendererAPI.h"
#include "Timefall/Core/Core.h"

namespace Timefall
{
	class TF_API RenderCommand
	{
	public:
		inline static void Init() { s_RendererAPI->Init(); }

		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		inline static void Clear(const glm::vec4& color = glm::vec4(0)) { s_RendererAPI->Clear(color); }

		inline static void DrawIndexed(
			const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, uint32_t indexOffset = 0, uint32_t baseVertex = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, indexCount, indexOffset, baseVertex);
		}

		inline static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
		{
			s_RendererAPI->DrawLines(vertexArray, vertexCount);
		}

		static void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
		{
			s_RendererAPI->DrawArrays(vertexArray, vertexCount);
		}

		inline static void SetLineWidth(float width) { s_RendererAPI->SetLineWidth(width); }

		inline static void SetDepthTest(bool enabled) { s_RendererAPI->SetDepthTest(enabled); }

		inline static void SetDepthWrite(bool enabled) { s_RendererAPI->SetDepthWrite(enabled); }

		inline static void SetDepthFunc(RendererAPI::DepthFunc func) { s_RendererAPI->SetDepthFunc(func); }

		inline static void SetFaceCulling(RendererAPI::FaceCull mode) { s_RendererAPI->SetFaceCulling(mode); }

	private:
		static Scope<RendererAPI> s_RendererAPI;
	};
}