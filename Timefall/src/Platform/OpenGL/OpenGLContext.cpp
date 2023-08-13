#include "tfpch.h"
#include "OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Timefall
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		TF_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		TF_CORE_ASSERT(success, "Faild to initialize Glad!");

		TF_CORE_INFO("OpenGL Info:");
		TF_CORE_INFO("	Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		TF_CORE_INFO("	Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		TF_CORE_INFO("	Version: {0}", (const char*)glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}
