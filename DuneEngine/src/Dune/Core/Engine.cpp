#include "pch.h"
#include "Dune/Core/Engine.h"
#include "Dune/Core/Graphics.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics/Window.h"
#include <Dune/Core/Graphics/Shaders/PBR.h>
#include <Dune/Utilities/DDSLoader.h>
#include <Dune/Utilities/SimpleCameraController.h>

namespace Dune
{
	struct OnResizeData
	{
		Pool<SceneView>* pPool;
		Handle<SceneView> hView;
	};

	struct SceneView
	{
		Graphics::View* m_pView{ nullptr };
		Graphics::DirectCommand* m_pCommand{ nullptr };

		SimpleCameraController m_cameraController;

		Handle<Graphics::Texture> m_depthBuffer;
		Handle<Graphics::Buffer> m_globalsBuffer;

		OnResizeData* pOnResizeData{ nullptr };
	};

	void OnResize(Graphics::View* pView, void* pData)
	{
		OnResizeData& data = *(OnResizeData*)pData;
		const Dune::Graphics::Window* pWindow{ pView->GetWindow() };
		dU32 width = pWindow->GetWidth();
		dU32 height = pWindow->GetHeight();

		SceneView& view = data.pPool->Get(data.hView);
		SimpleCameraController& cameraController = view.m_cameraController;
		cameraController.SetAspectRatio((float)width / (float)height);

		if (view.m_depthBuffer.IsValid())
		{
			Graphics::Device* pDevice{ pView->GetDevice() };
			Graphics::ReleaseTexture(pDevice, view.m_depthBuffer);
			view.m_depthBuffer = Graphics::CreateTexture(pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { width, height, 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = { 1.f, 1.f, 1.f, 1.f } });
		}
	}

	void Renderer::Initialize()
	{
		m_pDevice = Graphics::CreateDevice();
		m_sceneViewPool.Initialize(32);
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
						{.type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::All },
						{.type = Graphics::EBindingType::Group, .groupDesc = {.resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel },
						{.type = Graphics::EBindingType::Group, .groupDesc = {.resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel },
						{.type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::Vertex },
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
	}

	void Renderer::Destroy()
	{
		Graphics::ReleasePipeline(m_pDevice, m_pbrPipeline);
		m_sceneViewPool.Destroy();
		Graphics::DestroyDevice(m_pDevice);
		m_pDevice = nullptr;
	}

	bool Renderer::UpdateSceneView(Handle<SceneView> hView, float dt)
	{
		SceneView& view = m_sceneViewPool.Get(hView);
		if (Graphics::ProcessViewEvents(view.m_pView))
		{
			view.m_cameraController.Update(dt, view.m_pView->GetWindow()->GetInput());
			return true;
		}
		return false;
	}

	Handle<SceneView> Renderer::CreateSceneView()
	{
		Handle<SceneView> hView = m_sceneViewPool.Create();
		SceneView& view = m_sceneViewPool.Get(hView);

		Handle<Graphics::Texture> depthBuffer;
		view.m_cameraController = SimpleCameraController{ {.position = { 0.0f, 0.0f, 0.0f }, .target = { 0.0f, 0.0f, 1.0f }, } };
		view.pOnResizeData = new OnResizeData{ .pPool = &m_sceneViewPool, .hView = hView };
		view.m_pView = Graphics::CreateView({ .pDevice = m_pDevice, .pOnResize = OnResize, .pOnResizeData = view.pOnResizeData });
		view.m_pCommand = Graphics::CreateDirectCommand({ .pView = view.m_pView });
		const Dune::Graphics::Window* pWindow{ view.m_pView->GetWindow() };
		view.m_depthBuffer = Graphics::CreateTexture(m_pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { pWindow->GetWidth(), pWindow->GetHeight(), 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = {1.f, 1.f, 1.f, 1.f} });

		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(view.m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));
		view.m_globalsBuffer = Graphics::CreateBuffer(m_pDevice, { .debugName = L"GlobalsBuffer", .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::CPU, .pData = &globals, .byteSize = sizeof(Graphics::PBRGlobals) });
						
		return hView;
	}

	void Renderer::DestroySceneView(Handle<SceneView> hView)
	{
		SceneView& view = m_sceneViewPool.Get(hView);
		delete view.pOnResizeData;
		Graphics::ReleaseBuffer(m_pDevice, view.m_globalsBuffer);
		Graphics::ReleaseTexture(m_pDevice, view.m_depthBuffer);
		Graphics::DestroyCommand(view.m_pCommand);
		Graphics::DestroyView(view.m_pView);
		m_sceneViewPool.Remove(hView);
	}

	Handle<Graphics::Texture> Renderer::CreateTexture(const char* path)
	{
		return Graphics::DDSTexture::CreateTextureFromFile(m_pDevice, path);
	}

	void Renderer::ReleaseTexture(Handle<Graphics::Texture> hTexture)
	{
		Graphics::ReleaseTexture(m_pDevice, hTexture);
	}

	Handle<Graphics::Mesh> Renderer::CreateMesh(const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		return Graphics::CreateMesh(m_pDevice, pIndices, indexCount, pVertices, vertexCount, vertexByteStride);
	}

	void Renderer::ReleaseMesh(Handle<Graphics::Mesh> hMesh)
	{
		Graphics::ReleaseMesh(m_pDevice, hMesh);
	}

	void Renderer::RenderScene(Handle<SceneView> hView, const Scene& scene)
	{
		SceneView& view = m_sceneViewPool.Get(hView);
		Graphics::BeginFrame(view.m_pView);
		Graphics::ResetCommand(view.m_pCommand, m_pbrPipeline);
		Graphics::SetRenderTarget(view.m_pCommand, view.m_pView, view.m_depthBuffer);
		Graphics::ClearRenderTarget(view.m_pCommand, view.m_pView);
		Graphics::ClearDepthBuffer(view.m_pCommand, view.m_depthBuffer);
		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(view.m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));
		Graphics::MapBuffer(m_pDevice, view.m_globalsBuffer, &globals, 0, sizeof(Graphics::PBRGlobals));
		Graphics::PushGraphicsBuffer(view.m_pCommand, 0, view.m_globalsBuffer);

		dVector<Handle<Graphics::Buffer>> buffers{};

		scene.m_registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				const Graphics::Mesh& mesh = Graphics::GetMesh(m_pDevice, renderData.mesh);
				dMatrix initialModel{ DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position)) };
				Handle<Graphics::Buffer> instanceData = Graphics::CreateBuffer(m_pDevice, { .debugName = L"InstanceBuffer", .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::CPU, .byteSize = sizeof(Graphics::PBRInstance) });
				buffers.push_back(instanceData);
				Graphics::MapBuffer(m_pDevice, instanceData, &initialModel, 0, sizeof(Graphics::PBRInstance));
				Graphics::BindGraphicsTexture(view.m_pCommand, 1, renderData.albedo);
				Graphics::BindGraphicsTexture(view.m_pCommand, 2, renderData.normal);
				Graphics::PushGraphicsBuffer(view.m_pCommand, 3, instanceData);
				Graphics::BindIndexBuffer(view.m_pCommand, mesh.GetIndexBufferHandle());
				Graphics::BindVertexBuffer(view.m_pCommand, mesh.GetVertexBufferHandle());
				Graphics::DrawIndexedInstanced(view.m_pCommand, mesh.GetIndexCount(), 1);
			});

		Graphics::SubmitCommand(m_pDevice, view.m_pCommand);
		Graphics::EndFrame(view.m_pView);

		for (Handle<Graphics::Buffer> buf : buffers)
			Graphics::ReleaseBuffer(m_pDevice, buf);
	}
}
