#include "tfpch.h"
#include "EngineNative.h"

namespace Timefall
{
	extern "C"
	{
		__declspec(dllexport)
		void CppFunc()
		{
			std::cout << "CppFunc!" << std::endl;
		}
	}
}