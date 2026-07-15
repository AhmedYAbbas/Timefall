#include "tfpch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <tracy/TracyOpenGL.hpp>

namespace Timefall
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		TF_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		TF_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		TF_CORE_ASSERT(success, "Faild to initialize Glad!");

		TF_CORE_INFO("OpenGL Info:");
		TF_CORE_INFO("	Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		TF_CORE_INFO("	Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		TF_CORE_INFO("	Version: {0}", (const char*)glGetString(GL_VERSION));

#ifdef TF_ENABLE_ASSERTS
		int versionMajor;
		int versionMinor;
		glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
		glGetIntegerv(GL_MINOR_VERSION, &versionMinor);

		TF_CORE_ASSERT(versionMajor > 4 || (versionMajor == 4 && versionMinor >= 5), "Hazel requires at least OpenGL version 4.5!");
#endif

		TracyGpuContext;
	}

	void OpenGLContext::SwapBuffers()
	{
		TF_PROFILE_FUNCTION();

		glfwSwapBuffers(m_WindowHandle);
		TracyGpuCollect;

#ifdef TRACY_ENABLE
		// NVX_gpu_memory_info: driver-reported board usage (NVIDIA only, best-effort).
		static const bool hasMemoryInfo = glfwExtensionSupported("GL_NVX_gpu_memory_info") == GLFW_TRUE;
		if (hasMemoryInfo)
		{
			static double lastSample = 0.0;
			double now = glfwGetTime();
			if (now - lastSample > 1.0)
			{
				lastSample = now;
				constexpr GLenum GPU_MEMORY_INFO_TOTAL_AVAILABLE_NVX = 0x9048;
				constexpr GLenum GPU_MEMORY_INFO_CURRENT_AVAILABLE_NVX = 0x9049;
				GLint totalKB = 0, availableKB = 0;
				glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_NVX, &totalKB);
				glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_NVX, &availableKB);
				TF_PROFILE_PLOT("VRAM Board Used", (int64_t)(totalKB - availableKB) * 1024);
			}
		}
#endif
	}
}
