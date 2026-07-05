#pragma once

#include <glm/glm.hpp>

#include "Timefall/Renderer/VertexArray.h"

namespace Timefall
{
	class TF_API RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1
		};

		enum class FaceCull
		{
			None = 0, Front, Back
		};

	public:
		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void Clear(const glm::vec4& color) = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0,
			uint32_t indexOffset = 0, uint32_t baseVertex = 0) = 0;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		virtual void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;

		virtual void SetLineWidth(float width) = 0;

		virtual void SetDepthTest(bool enabled) = 0;
		virtual void SetDepthWrite(bool enabled) = 0;
		virtual void SetFaceCulling(FaceCull mode) = 0;

		inline static API GetAPI() { return s_API; }
		static Scope<RendererAPI> Create();

	private:
		static API s_API;
	};
}