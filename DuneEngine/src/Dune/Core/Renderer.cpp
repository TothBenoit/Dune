#include "pch.h"
#include "Dune/Core/Renderer.h"
#include "Dune/Core/Graphics.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics/Window.h"
#include <Dune/Core/Graphics/Shaders/PBR.h>

namespace Dune
{
	struct OnResizeData
	{
		Renderer& renderer;
	};

	void OnResize(Graphics::View* pView, void* pData)
	{
		OnResizeData& data = *(OnResizeData*)pData;
		const Dune::Graphics::Window* pWindow{ pView->GetWindow() };
		dU32 width = pWindow->GetWidth();
		dU32 height = pWindow->GetHeight();

		Renderer& renderer = data.renderer;
		renderer.OnResize(width, height);
	}

	void Renderer::Initialize(Graphics::Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-Zi", L"-I Shaders\\", L"-all_resources_bound" };

		Handle<Graphics::Shader> pbrVertexShader =
			Graphics::CreateShader
			(m_pDevice, {
				.stage = Graphics::EShaderStage::Vertex,
				.filePath = L"Shaders\\PBR.hlsl",
				.entryFunc = L"VSMain",
				.args = args,
				.argsCount = _countof(args),
				});

		Handle<Graphics::Shader> pbrPixelShader =
			Graphics::CreateShader
			(m_pDevice, {
				.stage = Graphics::EShaderStage::Pixel,
				.filePath = L"Shaders\\PBR.hlsl",
				.entryFunc = L"PSMain",
				.args = args,
				.argsCount = _countof(args),
				});

		m_pbrPipeline =
			Graphics::CreateGraphicsPipeline
			(m_pDevice, {
				.vertexShader = pbrVertexShader,
				.pixelShader = pbrPixelShader,
				.bindingLayout =
				{
					.slots =
					{
						{.type = Graphics::EBindingType::Constant, .byteSize = sizeof(Graphics::PBRGlobals), .visibility = Graphics::EShaderVisibility::All},
						{.type = Graphics::EBindingType::Group, .groupDesc = {.resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel },
						{.type = Graphics::EBindingType::Group, .groupDesc = {.resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel },
						{.type = Graphics::EBindingType::Constant, .byteSize = sizeof(Graphics::PBRInstance), .visibility = Graphics::EShaderVisibility::Vertex},
					},
					.slotCount = 4
				},
				.inputLayout =
					{
						Graphics::VertexInput {.pName = "POSITION", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
						Graphics::VertexInput {.pName = "NORMAL", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .bPerInstance = false },
						Graphics::VertexInput {.pName = "TANGENT", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .bPerInstance = false },
						Graphics::VertexInput {.pName = "UV", .index = 0, .format = Graphics::EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 36, .bPerInstance = false }
					},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.renderTargetsFormat = { Graphics::EFormat::R8G8B8A8_UNORM },
				.depthStencilFormat = Graphics::EFormat::D16_UNORM,
				});

		Graphics::ReleaseShader(m_pDevice, pbrPixelShader);
		Graphics::ReleaseShader(m_pDevice, pbrVertexShader);

		Handle<Graphics::Texture> depthBuffer;
		m_pOnResizeData = new OnResizeData{ .renderer = *this };
		m_pView = Graphics::CreateView({ .pDevice = m_pDevice, .pOnResize = Dune::OnResize, .pOnResizeData = m_pOnResizeData });
		m_pCommand = Graphics::CreateDirectCommand({ .pView = m_pView });
		const Dune::Graphics::Window* pWindow{ m_pView->GetWindow() };
		m_depthBuffer = Graphics::CreateTexture(m_pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { pWindow->GetWidth(), pWindow->GetHeight(), 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = {1.f, 1.f, 1.f, 1.f} });

		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		m_globalsBuffer = Graphics::CreateBuffer(m_pDevice, { .debugName = L"GlobalsBuffer", .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::CPU, .pData = &globals, .byteSize = sizeof(Graphics::PBRGlobals) });
	}

	void Renderer::Destroy()
	{
		Graphics::ReleasePipeline(m_pDevice, m_pbrPipeline);
		delete m_pOnResizeData;
		Graphics::ReleaseBuffer(m_pDevice, m_globalsBuffer);
		Graphics::ReleaseTexture(m_pDevice, m_depthBuffer);
		Graphics::DestroyCommand(m_pCommand);
		Graphics::DestroyView(m_pView);
	}

	void Renderer::OnResize(dU32 width, dU32 height) 
	{
		m_cameraController.SetAspectRatio((float)width / (float)height);

		if (m_depthBuffer.IsValid())
		{
			Graphics::ReleaseTexture(m_pDevice, m_depthBuffer);
			m_depthBuffer = Graphics::CreateTexture(m_pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { width, height, 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = { 1.f, 1.f, 1.f, 1.f } });
		}
	}

	bool Renderer::UpdateSceneView(float dt)
	{
		if (Graphics::ProcessViewEvents(m_pView))
		{
			m_cameraController.Update(dt, m_pView->GetWindow()->GetInput());
			return true;
		}
		return false;
	}

	void Renderer::RenderScene(const Scene& scene)
	{
		Graphics::BeginFrame(m_pView);
		Graphics::ResetCommand(m_pCommand, m_pbrPipeline);
		Graphics::SetRenderTarget(m_pCommand, m_pView, m_depthBuffer);
		Graphics::ClearRenderTarget(m_pCommand, m_pView);
		Graphics::ClearDepthBuffer(m_pCommand, m_depthBuffer);
		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));
		Graphics::PushGraphicsConstants(m_pCommand, 0, &globals, sizeof(Graphics::PBRGlobals));

		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				const Graphics::Mesh& mesh = Graphics::GetMesh(m_pDevice, renderData.mesh);
				Graphics::PBRInstance instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix, DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position)));
				Graphics::BindGraphicsTexture(m_pCommand, 1, renderData.albedo);
				Graphics::BindGraphicsTexture(m_pCommand, 2, renderData.normal);
				Graphics::PushGraphicsConstants(m_pCommand, 3, &instance, sizeof(Graphics::PBRInstance));
				Graphics::BindIndexBuffer(m_pCommand, mesh.GetIndexBufferHandle());
				Graphics::BindVertexBuffer(m_pCommand, mesh.GetVertexBufferHandle());
				Graphics::DrawIndexedInstanced(m_pCommand, mesh.GetIndexCount(), 1);
			});

		Graphics::SubmitCommand(m_pDevice, m_pCommand);
		Graphics::EndFrame(m_pView);
	}
}
