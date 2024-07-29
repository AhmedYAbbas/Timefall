#include "tfpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include "Timefall/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace Timefall
{
    void OpenGLMessageCallback(
        unsigned source,
        unsigned type,
        unsigned id,
        unsigned severity,
        int length,
        const char* message,
        const void* userParam)
    {
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
                TF_CORE_CRITICAL(message);
                return;
            case GL_DEBUG_SEVERITY_MEDIUM:
                TF_CORE_ERROR(message);
                return;
            case GL_DEBUG_SEVERITY_LOW:
                TF_CORE_WARN(message);
                return;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                TF_CORE_TRACE(message);
                return;
        }

        TF_CORE_ASSERT(false, "Unknown severity level!");
    }

    void OpenGLRendererAPI::Init()
    {
        TF_PROFILE_FUNCTION();

    #ifdef TF_DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLMessageCallback, nullptr);

        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
    #endif

        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
    {
        TF_PROFILE_FUNCTION();

        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}