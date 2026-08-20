#include "tfpch.h"

#include "Timefall/Renderer/Renderer.h"
#include "Timefall/Renderer/Renderer2D.h"
#include "Timefall/Renderer/Renderer3D.h"

namespace Timefall
{
	Scope<Renderer::SceneData> Renderer::s_SceneData = CreateScope<SceneData>();

	void Renderer::Init()
	{
		TF_PROFILE_FUNCTION();

		Renderer2D::Init();
		Renderer3D::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
		Renderer3D::Shutdown();
	}

	// Viewport is per-pass state on the CommandList now; nothing global to resize.
	void Renderer::OnWindowResize(uint32_t, uint32_t) {}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjection();
	}

	void Renderer::EndScene() {}
}
