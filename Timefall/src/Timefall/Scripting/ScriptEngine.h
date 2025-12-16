#pragma once

namespace Timefall
{
	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

	private:
		static bool LoadHostFxr();
		static void ShutdownHostFxr();
	};
}