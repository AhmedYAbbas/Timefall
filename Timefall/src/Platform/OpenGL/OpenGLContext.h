#pragma once

#include "Timefall/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Timefall
{

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);

		void Init() override;
		void SwapBuffers() override;

	private:
		GLFWwindow* m_WindowHandle;
	};
}
