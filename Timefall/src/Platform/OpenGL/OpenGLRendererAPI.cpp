#include "tfpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include "Timefall/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace Timefall
{
    void OpenGLRendererAPI::Init()
    {
        TF_PROFILE_FUNCTION();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        TF_PROFILE_FUNCTION();

        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::Clear(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
    {
        TF_PROFILE_FUNCTION();

        glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}