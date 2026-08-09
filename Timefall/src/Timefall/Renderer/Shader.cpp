#include "tfpch.h"

#include "Timefall/Renderer/Shader.h"

#include <filesystem>

namespace Timefall
{
	namespace
	{
		class StubShader : public Shader
		{
		public:
			explicit StubShader(std::string name)
				: m_Name(std::move(name))
			{}

			void Bind() const override {}
			void Unbind() const override {}

			void SetInt(const std::string&, int) override {}
			void SetIntArray(const std::string&, int*, uint32_t) override {}

			void SetFloat3(const std::string&, const glm::vec3) override {}
			void SetFloat(const std::string&, float) override {}
			void SetFloat4(const std::string&, const glm::vec4) override {}
			void SetMat3(const std::string&, const glm::mat3) override {}
			void SetMat4(const std::string&, const glm::mat4) override {}

			const std::string& GetName() const override { return m_Name; }

		private:
			std::string m_Name;
		};
	}

	Ref<Shader> Shader::Create(const std::filesystem::path& filepath)
	{
		return CreateRef<StubShader>(filepath.stem().string());
	}
}
