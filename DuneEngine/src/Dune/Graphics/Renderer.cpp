#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Window.h"
#include <Dune/Graphics/Shaders/PBR.h>
#include <Dune/Graphics/RHI/Swapchain.h>
#include <Dune/Graphics/RHI/GraphicsPipeline.h>
#include <Dune/Graphics/RHI/Fence.h>
#include <Dune/Graphics/RHI/CommandList.h>
#include <Dune/Graphics/RHI/Barrier.h>
#include <Dune/Graphics/RHI/Shader.h>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/Format.h>


namespace Dune
{
	void Renderer::Initialize(Graphics::Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-Zi", L"-all_resources_bound" };

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

		m_pWindow = new Graphics::Window();
		m_pWindow->Initialize({});
		m_pDepthBuffer = new Graphics::Texture();
		m_pDepthBuffer->Initialize( 
			m_pDevice, 
			{ 
				.debugName = L"DepthBuffer", 
				.usage = Graphics::ETextureUsage::DepthStencil, 
				.dimensions = { m_pWindow->GetWidth(), m_pWindow->GetHeight(), 1}, 
				.format = Graphics::EFormat::D16_UNORM, 
				.clearValue = {1.f, 1.f, 1.f, 1.f}, 
				.initialState = Graphics::EResourceState::DepthStencil 
			}
		);

		m_pFence = new Graphics::Fence();
		m_pFence->Initialize(m_pDevice, 0);
		m_pCommandQueue = new Graphics::CommandQueue();
		m_pCommandQueue->Initialize(m_pDevice, Graphics::ECommandType::Direct);

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(m_pDevice, Graphics::ECommandType::Direct);
			frame.commandList.Initialize(m_pDevice, Graphics::ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
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

		for (dU32 i = 0; i < 3; i++)
		{
			Graphics::Descriptor rtv = m_pRtvHeap->Allocate();
			m_pDevice->CreateRTV(rtv, m_pSwapchain->GetBackBuffer(i), {});
			m_renderTargetsDescriptors[i] = rtv;
		}

		m_depthBufferDescriptor = m_pDsvHeap->Allocate();
		m_pDevice->CreateDSV(m_depthBufferDescriptor, *m_pDepthBuffer, {});

		m_frameIndex = m_pSwapchain->GetCurrentBackBufferIndex();
	}

	void Renderer::Destroy()
	{
		for (dU32 i = 0; i < 3; i++) 
		{
			Frame& frame = m_frames[i];
			while (!frame.descriptorsToRelease.empty())
			{
				m_pSrvHeap->Free(frame.descriptorsToRelease.front());
				frame.descriptorsToRelease.pop();
			}
			WaitForFrame(frame);
			m_pRtvHeap->Free(m_renderTargetsDescriptors[i]);
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
		}
		m_pDsvHeap->Free(m_depthBufferDescriptor);
		
		m_pSrvHeap->Destroy();
		delete m_pSrvHeap;
		m_pSamplerHeap->Destroy();
		delete m_pSamplerHeap;
		m_pRtvHeap->Destroy();
		delete m_pRtvHeap;
		m_pDsvHeap->Destroy();
		delete m_pDsvHeap;
		m_pBarrier->Destroy();
		delete m_pBarrier;
		m_pPbrPipeline->Destroy();
		delete m_pPbrPipeline;
		m_pDepthBuffer->Destroy();
		delete m_pDepthBuffer;

		m_pCommandQueue->Destroy();
		delete m_pCommandQueue;

		m_pSwapchain->Destroy();
		delete m_pSwapchain;

		m_pFence->Destroy();
		delete m_pFence;

		m_pWindow->Destroy();
		delete m_pWindow;
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

	void Renderer::WaitForFrame(const Frame& frame)
	{
		const dU64 fenceValue{ frame.fenceValue };
		if ( m_pFence->GetValue() < fenceValue )
			m_pFence->Wait(fenceValue);
	}

	void Renderer::RenderScene(const Scene& scene)
	{
		Frame& frame = m_frames[m_frameIndex];
		WaitForFrame(frame);
		while ( !frame.descriptorsToRelease.empty() )
		{
			m_pSrvHeap->Free(frame.descriptorsToRelease.front());
			frame.descriptorsToRelease.pop();
		}
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator, *m_pPbrPipeline);
		frame.commandList.SetDescriptorHeaps(*m_pSrvHeap, *m_pSamplerHeap);
		frame.commandList.SetPrimitiveTopology(Graphics::EPrimitiveTopology::TriangleList);
		Graphics::Viewport viewport { 0.0, 0.0, (float)m_pWindow->GetWidth(), (float)m_pWindow->GetHeight(), 0.0f, 1.0f };
		Graphics::Scissor scissor { 0, 0, m_pWindow->GetWidth(), m_pWindow->GetHeight() };
		frame.commandList.SetViewports(1, &viewport);
		frame.commandList.SetScissors(1, &scissor);
		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex).Get(), Graphics::EResourceState::Undefined, Graphics::EResourceState::RenderTarget);
		frame.commandList.Transition(*m_pBarrier);
		m_pBarrier->Reset();
		Graphics::Descriptor rtv = m_renderTargetsDescriptors[m_frameIndex];
		Graphics::Descriptor dsv = m_depthBufferDescriptor;
		frame.commandList.SetRenderTarget(&rtv.cpuAddress, 1, &dsv.cpuAddress);
		const float color[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		frame.commandList.ClearRenderTargetView(rtv, color);
		frame.commandList.ClearDepthBuffer(dsv, m_pDepthBuffer->GetClearValue()[0], 0);

		Graphics::PBRGlobals globals;
		Graphics::ComputeViewProjectionMatrix(m_cameraController.GetCamera(), nullptr, nullptr, &globals.viewProjectionMatrix);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));

		frame.commandList.PushGraphicsConstants(0, &globals, sizeof(Graphics::PBRGlobals));

		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				const Graphics::Mesh& mesh = *renderData.pMesh;
				Graphics::PBRInstance instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix, DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position)));

				Graphics::Descriptor albedo = m_pSrvHeap->Allocate();
				m_pDevice->CreateSRV(albedo, *renderData.pAlbedo, { .mipLevels = renderData.pAlbedo->GetMipLevels()});
				Graphics::Descriptor normal = m_pSrvHeap->Allocate();
				m_pDevice->CreateSRV(normal, *renderData.pNormal, { .mipLevels = renderData.pNormal->GetMipLevels() });

				frame.commandList.BindGraphicsResource(1, albedo);
				frame.commandList.BindGraphicsResource(2, normal);
				frame.commandList.PushGraphicsConstants(3, &instance, sizeof(Graphics::PBRInstance));
				frame.commandList.BindIndexBuffer(mesh.GetIndexBuffer());
				frame.commandList.BindVertexBuffer(mesh.GetVertexBuffer());
				frame.commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);

				frame.descriptorsToRelease.push(albedo);
				frame.descriptorsToRelease.push(normal);
			});

		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex).Get(), Graphics::EResourceState::RenderTarget, Graphics::EResourceState::Undefined);
		frame.commandList.Transition(*m_pBarrier);
		m_pBarrier->Reset();
		frame.commandList.Close();
		m_pCommandQueue->ExecuteCommandLists(&frame.commandList, 1);
		m_pSwapchain->Present();
		m_pCommandQueue->Signal(*m_pFence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % 3;
	}
}
