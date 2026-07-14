#include "tfpch.h"
#include "Platform/OpenGL/OpenGLEnvironment.h"

#include "Timefall/Renderer/RenderCommand.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace Timefall
{
	static const glm::mat4 s_CaptureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	static const glm::mat4 s_CaptureViews[6] = {
		glm::lookAt(glm::vec3(0.0f), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)),
	};

	Ref<Environment> Environment::Create(const Ref<Texture2D>& equirect)
	{
		return CreateRef<OpenGLEnvironment>(equirect);
	}

	OpenGLEnvironment::OpenGLEnvironment(const Ref<Texture2D>& equirect)
	{
		m_CubeMesh = MeshSource::CreateCube();
		glCreateFramebuffers(1, &m_CaptureFBO);

		const uint32_t envSize = 512;
		uint32_t envMips = 1 + (uint32_t)std::floor(std::log2((float)envSize));
		m_Skybox = TextureCube::Create(envSize, envMips);

		Ref<Shader> equirectShader = Shader::Create("assets/shaders/Renderer3D_EquirectToCube.glsl");
		equirectShader->Bind();
		equirectShader->SetInt("u_Equirect", 0);
		equirect->Bind(0);

		GLint prevFBO;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
		GLint prevVP[4];
		glGetIntegerv(GL_VIEWPORT, prevVP);
		RenderCommand::SetDepthTest(false);
		RenderCommand::SetFaceCulling(RendererAPI::FaceCull::None);

		glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
		glViewport(0, 0, envSize, envSize);
		const Submesh& sm = m_CubeMesh->GetSubmeshes()[0];
		for (uint32_t face = 0; face < 6; ++face)
		{
			equirectShader->SetMat4("u_ViewProjection", s_CaptureProj * s_CaptureViews[face]);
			glNamedFramebufferTextureLayer(m_CaptureFBO, GL_COLOR_ATTACHMENT0, m_Skybox->GetRendererID(), 0, (GLint)face);
			glClear(GL_COLOR_BUFFER_BIT);
			RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
		}
		// Env-map mips feed the specular prefilter's solid-angle sampling (Tasks 6-7).
		glGenerateTextureMipmap(m_Skybox->GetRendererID());

		// --- Diffuse irradiance (cosine convolution of the env cubemap) ---
		const uint32_t irrSize = 32;
		m_Irradiance = TextureCube::Create(irrSize, 1);
		{
			Ref<Shader> irrShader = Shader::Create("assets/shaders/Renderer3D_IrradianceConvolve.glsl");
			irrShader->Bind();
			irrShader->SetInt("u_EnvMap", 0);
			m_Skybox->BindForSampling(0);
			glViewport(0, 0, irrSize, irrSize);
			for (uint32_t face = 0; face < 6; ++face)
			{
				irrShader->SetMat4("u_ViewProjection", s_CaptureProj * s_CaptureViews[face]);
				glNamedFramebufferTextureLayer(m_CaptureFBO, GL_COLOR_ATTACHMENT0, m_Irradiance->GetRendererID(), 0, (GLint)face);
				glClear(GL_COLOR_BUFFER_BIT);
				RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
			}
		}

		// --- Prefiltered specular (GGX importance sampling, roughness per mip) ---
		const uint32_t preSize = 128;
		m_PrefilterMips = 5;
		m_Prefilter = TextureCube::Create(preSize, m_PrefilterMips);
		{
			Ref<Shader> preShader = Shader::Create("assets/shaders/Renderer3D_Prefilter.glsl");
			preShader->Bind();
			preShader->SetInt("u_EnvMap", 0);
			preShader->SetFloat("u_EnvResolution", (float)envSize);
			m_Skybox->BindForSampling(0);
			for (uint32_t mip = 0; mip < m_PrefilterMips; ++mip)
			{
				uint32_t mipSize = preSize >> mip;
				glViewport(0, 0, (GLsizei)mipSize, (GLsizei)mipSize);
				float roughness = (float)mip / (float)(m_PrefilterMips - 1);
				preShader->SetFloat("u_Roughness", roughness);
				for (uint32_t face = 0; face < 6; ++face)
				{
					preShader->SetMat4("u_ViewProjection", s_CaptureProj * s_CaptureViews[face]);
					glNamedFramebufferTextureLayer(
						m_CaptureFBO, GL_COLOR_ATTACHMENT0, m_Prefilter->GetRendererID(), (GLint)mip, (GLint)face);
					glClear(GL_COLOR_BUFFER_BIT);
					RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
				}
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
		glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
		RenderCommand::SetDepthTest(true);
		RenderCommand::SetFaceCulling(RendererAPI::FaceCull::Back);
	}

	OpenGLEnvironment::~OpenGLEnvironment()
	{
		glDeleteFramebuffers(1, &m_CaptureFBO);
	}
}
