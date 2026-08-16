#include "tfpch.h"
#include "RenderDocCapture.h"

#include <renderdoc_app.h>


namespace Timefall
{
	static RENDERDOC_API_1_6_0* s_API = nullptr;
	static bool s_Probed = false;

	RENDERDOC_API_1_6_0* Api()
	{
		if (s_Probed)
			return s_API;

		s_Probed = true;

		// GetModuleHandle, never LoadLibrary: if RenderDoc is not already in the process it
		// missed the loader hook, and loading it now would capture nothing
		HMODULE module = GetModuleHandleA("renderdoc.dll");
		if (!module)
			return nullptr;

		auto getApi = (pRENDERDOC_GetAPI)GetProcAddress(module, "RENDERDOC_GetAPI");
		if (!getApi || getApi(eRENDERDOC_API_Version_1_6_0, (void**)&s_API) != 1)
		{
			s_API = nullptr;
			return nullptr;
		}

		TF_CORE_INFO("RenderDoc attached; in-app capture avaiable");
		return s_API;
	}

	bool RenderDocCapture::IsAttached()
	{
		return Api() != nullptr;
	}

	void RenderDocCapture::TriggerCapture()
	{
		if (auto* api = Api())
			api->TriggerCapture();
	}
}
