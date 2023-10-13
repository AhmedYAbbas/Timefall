#include "tfpch.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Timefall/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace Timefall
{
    void OpenGLRendererAPI::Clear(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray)
    {
        glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
    }
}