#include "pch.h"
#include "Dune/Core/Renderer.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics/Window.h"
#include <Dune/Core/Graphics/Shaders/PBR.h>
#include <Dune/Core/Graphics/RHI/Swapchain.h>
#include <Dune/Core/Graphics/RHI/GraphicsPipeline.h>
#include <Dune/Core/Graphics/RHI/Fence.h>
#include <Dune/Core/Graphics/RHI/CommandList.h>
#include <Dune/Core/Graphics/RHI/Barrier.h>
#include <Dune/Core/Graphics/RHI/Shader.h>
#include <Dune/Core/Graphics/RHI/Texture.h>
#include <Dune/Core/Graphics/Format.h>


namespace Dune
{
	struct OnResizeData
	{
		Renderer& renderer;
	};

	void Renderer::Initialize(Graphics::Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-Zi", L"-I Shaders\\", L"-all_resources_bound" };

		Graphics::Shader pbrVertexShader;
		pbrVertexShader.Initialize
			({
				.stage = Graphics::EShaderStage::Vertex,
				.filePath = L"Shaders\\PBR.hlsl",
				.entryFunc = L"VSMain",
				.args = args,
				.argsCount = _countof(args),
			});

		Graphics::Shader pbrPixelShader;
		pbrPixelShader.Initialize
			({
				.stage = Graphics::EShaderStage::Pixel,
				.filePath = L"Shaders\\PBR.hlsl",
				.entryFunc = L"PSMain",
				.args = args,
				.argsCount = _countof(args),
			});

		m_pPbrPipeline = new Graphics::GraphicsPipeline();
		m_pPbrPipeline->Initialize
			(m_pDevice, {
				.pVertexShader = &pbrVertexShader,
				.pPixelShader = &pbrPixelShader,
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
				.renderTargetCount = 1,
				.renderTargetsFormat = { Graphics::EFormat::R8G8B8A8_UNORM },
				.depthStencilFormat = Graphics::EFormat::D16_UNORM,
				});

		pbrVertexShader.Destroy();
		pbrPixelShader.Destroy();

		m_pOnResizeData = new OnResizeData{ .renderer = *this };
		m_pWindow = new Graphics::Window();
		m_pWindow->Initialize({});
		m_pDepthBuffer = new Graphics::Texture();
		m_pDepthBuffer->Initialize( m_pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DepthStencil, .dimensions = { m_pWindow->GetWidth(), m_pWindow->GetHeight(), 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = {1.f, 1.f, 1.f, 1.f} });

		m_pFence = new Graphics::Fence();
		m_pFence->Initialize(m_pDevice, 0);
		m_pCommandQueue = new Graphics::CommandQueue();
		m_pCommandQueue->Initialize(m_pDevice, Graphics::ECommandType::Direct);

		for (Frame& frame : m_frames)
		{
			frame.pCommandAllocator = new Graphics::CommandAllocator();
			frame.pCommandAllocator->Initialize(m_pDevice, Graphics::ECommandType::Direct);
			frame.pCommandList = new Graphics::CommandList();
			frame.pCommandList->Initialize(m_pDevice, Graphics::ECommandType::Direct, *frame.pCommandAllocator);
			frame.pCommandList->Close();
		}
		m_pBarrier = new Graphics::Barrier();
		m_pBarrier->Initialize(16);

		m_pSwapchain = new Graphics::Swapchain();
		m_pSwapchain->Initialize(m_pDevice, m_pWindow, m_pCommandQueue, { .latency = 3 });

		Graphics::DescriptorHeapDesc heapDesc = { .type = Graphics::DescriptorHeapType::SRV_CBV_UAV, .capacity = 4096 };
		m_pSrvHeap = new Graphics::DescriptorHeap();
		m_pSrvHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = Graphics::DescriptorHeapType::Sampler;
		m_pSamplerHeap = new Graphics::DescriptorHeap();
		m_pSamplerHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.type = Graphics::DescriptorHeapType::RTV;
		m_pRtvHeap = new Graphics::DescriptorHeap();
		m_pRtvHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.type = Graphics::DescriptorHeapType::DSV;
		m_pDsvHeap = new  Graphics::DescriptorHeap();
		m_pDsvHeap->Initialize(m_pDevice, heapDesc);
	}

	void Renderer::Destroy()
	{
		delete m_pSrvHeap;
		delete m_pSamplerHeap;
		delete m_pRtvHeap;
		delete m_pDsvHeap;
		delete m_pBarrier;
		for (Frame& frame : m_frames)
		{
			delete frame.pCommandAllocator;
			delete frame.pCommandList;
		}
		delete m_pPbrPipeline;
		delete m_pOnResizeData;
		delete m_pDepthBuffer;

		m_pSwapchain->Destroy();
		delete m_pSwapchain;
	}

	void Renderer::OnResize(dU32 width, dU32 height) 
	{
		m_cameraController.SetAspectRatio((float)width / (float)height);

		if (m_pDepthBuffer)
		{
			m_pDepthBuffer->Destroy();
			m_pDepthBuffer->Initialize(m_pDevice, { .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DepthStencil, .dimensions = { width, height, 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = { 1.f, 1.f, 1.f, 1.f } });
		}
	}

	bool Renderer::UpdateSceneView(float dt)
	{
		if (m_pWindow->Update())
		{
			m_cameraController.Update(dt, m_pWindow->GetInput());
			return true;
		}
		return false;
	}

	void Renderer::WaitForFrame(const dU32 frameIndex)
	{
		const dU64 fenceValue{ m_frames[frameIndex].fenceValue };
		if ( m_pFence->GetValue() < fenceValue )
			m_pFence->Wait(fenceValue);
	}

	void Renderer::RenderScene(const Scene& scene)
	{
		WaitForFrame(m_frameIndex);
		Frame& frame = m_frames[m_frameIndex];
		frame.pCommandAllocator->Reset();
		frame.pCommandList->Reset(*frame.pCommandAllocator, *m_pPbrPipeline);
		frame.pCommandList->SetDescriptorHeaps(*m_pSrvHeap, *m_pSamplerHeap);
		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex), Graphics::EResourceState::Undefined, Graphics::EResourceState::RenderTarget);
		frame.pCommandList->Transition(*m_pBarrier);
		Graphics::Descriptor rtv = m_pRenderTargetsDescriptors[m_frameIndex];
		Graphics::Descriptor dsv = m_depthBufferDescriptor;
		frame.pCommandList->SetRenderTarget(&rtv.cpuAddress, 1, &dsv.cpuAddress);
		const float color[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		frame.pCommandList->ClearRenderTargetView(rtv, color);
		frame.pCommandList->ClearDepthBuffer(dsv, m_pDepthBuffer->GetClearValue()[0], 0);

		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));

		frame.pCommandList->PushGraphicsConstants(0, &globals, sizeof(Graphics::PBRGlobals));

		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				const Graphics::Mesh& mesh = *renderData.pMesh;
				Graphics::PBRInstance instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix, DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position)));
				frame.pCommandList->BindGraphicsResource(1, renderData.albedo);
				frame.pCommandList->BindGraphicsResource(2, renderData.normal);
				frame.pCommandList->PushGraphicsConstants(3, &instance, sizeof(Graphics::PBRInstance));
				frame.pCommandList->BindIndexBuffer(*mesh.GetIndexBuffer());
				frame.pCommandList->BindVertexBuffer(*mesh.GetVertexBuffer());
				frame.pCommandList->DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
			});

		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex), Graphics::EResourceState::RenderTarget, Graphics::EResourceState::Undefined);
		frame.pCommandList->Transition(*m_pBarrier);
		m_pSwapchain->Present();
		m_pCommandQueue->ExecuteCommandLists(frame.pCommandList, 1);
		m_pCommandQueue->Signal(*m_pFence, ++frame.fenceValue);
		m_frameIndex = (m_frameIndex + 1) % 3;
	}
}
