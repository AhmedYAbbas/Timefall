#include "tfpch.h"
#include "EngineNative.h"

namespace Timefall
{
	extern "C"
	{
		__declspec(dllexport)
		void NativeLog(const char* str, int parameter)
		{
			std::cout << str << ", " << parameter << '\n';
		}

		__declspec(dllexport)
		void NativeLog_Vector(glm::vec3* vec, glm::vec3* outResult)
		{
			std::cout << *vec << '\n';

			*outResult = glm::normalize(*vec);
		}
		
		__declspec(dllexport)
		float NativeLog_VectorDot(glm::vec3* vec)
		{
			std::cout << *vec << '\n';
			return glm::dot(*vec, *vec);
		}
	}
}