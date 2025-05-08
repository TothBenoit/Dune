#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Window.h"
#include <Dune/Graphics/RenderPass/Forward.h>
#include <Dune/Graphics/RenderPass/DepthPrepass.h>
#include <Dune/Graphics/Shaders/ShaderTypes.h>
#include <Dune/Graphics/RHI/Swapchain.h>
#include <Dune/Graphics/RHI/GraphicsPipeline.h>
#include <Dune/Graphics/RHI/Fence.h>
#include <Dune/Graphics/RHI/CommandList.h>
#include <Dune/Graphics/RHI/Barrier.h>
#include <Dune/Graphics/RHI/Shader.h>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/RHI/ImGUIWrapper.h>
#include <Dune/Graphics/Format.h>
#include <Dune/Scene/Scene.h>
#include <Dune/Scene/Camera.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx12.h>

namespace Dune::Graphics
{
	void Renderer::Initialize(Device& device, Window& window)
	{
		m_pDevice = &device;
		m_pWindow = &window;

		m_pDepthBuffer = new Texture();
		m_pDepthBuffer->Initialize( 
			m_pDevice, 
			{ 
				.debugName = L"DepthBuffer", 
				.usage = ETextureUsage::DepthStencil, 
				.dimensions = { m_pWindow->GetWidth(), m_pWindow->GetHeight(), 1}, 
				.format = EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f}, 
				.initialState = EResourceState::DepthStencil 
			}
		);

		m_pFence = new Fence();
		m_pFence->Initialize(m_pDevice, 0);
		m_pCommandQueue = new CommandQueue();
		m_pCommandQueue->Initialize(m_pDevice, ECommandType::Direct);

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(m_pDevice, ECommandType::Direct);
			frame.commandList.Initialize(m_pDevice, ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
		}
		m_pBarrier = new Barrier();
		m_pBarrier->Initialize(16);

		m_pSwapchain = new Swapchain();
		m_pSwapchain->Initialize(m_pDevice, m_pWindow, m_pCommandQueue, { .latency = 3 });

		DescriptorHeapDesc heapDesc = { .type = DescriptorHeapType::SRV_CBV_UAV, .capacity = 4096 };
		m_pSrvHeap = new Graphics::DescriptorHeap();
		m_pSrvHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = DescriptorHeapType::Sampler;
		m_pSamplerHeap = new DescriptorHeap();
		m_pSamplerHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.type = DescriptorHeapType::RTV;
		m_pRtvHeap = new DescriptorHeap();
		m_pRtvHeap->Initialize(m_pDevice, heapDesc);

		heapDesc.type = DescriptorHeapType::DSV;
		m_pDsvHeap = new  DescriptorHeap();
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

		m_pForwardPass = new Forward();
		m_pForwardPass->Initialize(m_pDevice);

		m_pDepthPrepass = new DepthPrepass();
		m_pDepthPrepass->Initialize(m_pDevice);
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
			while (!frame.buffersToRelease.empty())
			{
				frame.buffersToRelease.front().Destroy();
				frame.buffersToRelease.pop();
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
		m_pForwardPass->Destroy();
		delete m_pForwardPass;
		m_pDepthPrepass->Destroy();
		delete m_pDepthPrepass;
		m_pDepthBuffer->Destroy();
		delete m_pDepthBuffer;

		m_pCommandQueue->Destroy();
		delete m_pCommandQueue;

		m_pSwapchain->Destroy();
		delete m_pSwapchain;

		m_pFence->Destroy();
		delete m_pFence;
	}

	void Renderer::OnResize(dU32 width, dU32 height) 
	{
		for ( const Frame& f : m_frames )
			WaitForFrame(f);
		m_pSwapchain->Resize(width, height);
		m_frameIndex = m_pSwapchain->GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < 3; i++)
			m_pDevice->CreateRTV(m_renderTargetsDescriptors[i], m_pSwapchain->GetBackBuffer(i), {});

		m_pDepthBuffer->Destroy();
		m_pDepthBuffer->Initialize(
			m_pDevice,
			{
				.debugName = L"DepthBuffer",
				.usage = ETextureUsage::DepthStencil,
				.dimensions = {width, height, 1},
				.format = EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = EResourceState::DepthStencil
			}
		);
		m_pDevice->CreateDSV(m_depthBufferDescriptor, *m_pDepthBuffer, {});
	}

	void Renderer::WaitForFrame(const Frame& frame)
	{
		const dU64 fenceValue{ frame.fenceValue };
		if ( m_pFence->GetValue() < fenceValue )
			m_pFence->Wait(fenceValue);
	}

	void Renderer::Render(Scene& scene, Camera& camera)
	{
		Frame& frame = m_frames[m_frameIndex];
		WaitForFrame(frame);
		while ( !frame.descriptorsToRelease.empty() )
		{
			m_pSrvHeap->Free(frame.descriptorsToRelease.front());
			frame.descriptorsToRelease.pop();
		}
		while (!frame.buffersToRelease.empty())
		{
			frame.buffersToRelease.front().Destroy();
			frame.buffersToRelease.pop();
		}

		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(*m_pSrvHeap, *m_pSamplerHeap);

		Buffer directionalLights{};
		dU32 directionalLightCount = 0;

		{
			auto view = scene.registry.view<const DirectionalLight>();
			directionalLightCount = (dU32)view.size();
			dU32 byteSize = (dU32)((directionalLightCount == 0 ? 1 : directionalLightCount) * sizeof(DirectionalLight));
			directionalLights.Initialize(m_pDevice,
				{
					.debugName{ L"DirectitonalLightBuffer" },
					.usage{ EBufferUsage::Constant },
					.memory{ EBufferMemory::CPU },
					.byteSize{ byteSize },
					.initialState{ EResourceState::Undefined }
				});
			void* pData{ nullptr };
			directionalLights.Map(0, byteSize, &pData);
			dU32 count{ 0 };
			view.each([&](const DirectionalLight& light)
				{
					memcpy((DirectionalLight*)pData + count++, &light, sizeof(DirectionalLight));
				}
			);
			directionalLights.Unmap(0, byteSize);
		}
		frame.buffersToRelease.push(directionalLights);

		Buffer pointLights{};
		dU32 pointLightCount = 0;
		{
			auto view = scene.registry.view<const PointLight>();
			pointLightCount = (dU32)view.size();
			dU32 byteSize = (dU32)((pointLightCount == 0 ? 1 : pointLightCount) * sizeof(PointLight));
			pointLights.Initialize(m_pDevice,
				{
					.debugName{ L"PointLightBuffer" },
					.usage{ EBufferUsage::Constant },
					.memory{ EBufferMemory::CPU },
					.byteSize{ byteSize },
					.initialState{ EResourceState::Undefined }
				});
			void* pData{ nullptr };
			pointLights.Map(0, byteSize, &pData);
			dU32 count{ 0 };
			view.each([&](const PointLight& light)
				{
					memcpy((PointLight*)pData + count++, &light, sizeof(PointLight));
				}
			);
			pointLights.Unmap(0, byteSize);
		}
		frame.buffersToRelease.push(pointLights);

		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex).Get(), EResourceState::Present, EResourceState::RenderTarget);
		frame.commandList.Transition(*m_pBarrier);
		m_pBarrier->Reset();

		Descriptor rtv = m_renderTargetsDescriptors[m_frameIndex];
		Descriptor dsv = m_depthBufferDescriptor;

		const float color[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		frame.commandList.ClearRenderTargetView(rtv, color);
		frame.commandList.ClearDepthBuffer(dsv, m_pDepthBuffer->GetClearValue()[0], 0);

		Viewport viewport{ 0.0, 0.0, (float)m_pWindow->GetWidth(), (float)m_pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, m_pWindow->GetWidth(), m_pWindow->GetHeight() };
		frame.commandList.SetViewports(1, &viewport);
		frame.commandList.SetScissors(1, &scissor);
		
		frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
		m_pDepthPrepass->Render(scene, frame.commandList, camera);

		frame.commandList.SetRenderTarget(&rtv.cpuAddress, 1, &dsv.cpuAddress);
		m_pForwardPass->Render(scene, *m_pSrvHeap, frame.commandList, camera, directionalLights, pointLights, frame.descriptorsToRelease);

		if (m_pImGui)
			m_pImGui->Render(frame.commandList);

		m_pBarrier->PushTransition(m_pSwapchain->GetBackBuffer(m_frameIndex).Get(), EResourceState::RenderTarget, EResourceState::Present);
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
