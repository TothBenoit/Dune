#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/ImGUIWrapper.h"
#include <Dune/Graphics/Shaders/ShaderTypes.h>
#include "Dune/Scene/Scene.h"
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx12.h>

namespace Dune::Graphics
{
	void Renderer::Initialize(Device& device, Window& window)
	{
		m_pDevice = &device;
		m_pWindow = &window;

		m_depthBuffer.Initialize( 
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

		m_fence.Initialize(m_pDevice, 0);
		m_commandQueue.Initialize(m_pDevice, ECommandType::Direct);

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(m_pDevice, ECommandType::Direct);
			frame.commandList.Initialize(m_pDevice, ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
		}
		m_barrier.Initialize(16);
		m_swapchain.Initialize(m_pDevice, m_pWindow, &m_commandQueue, { .latency = 3 });

		DescriptorHeapDesc heapDesc = { .type = DescriptorHeapType::SRV_CBV_UAV, .capacity = 4096 };
		m_srvHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = DescriptorHeapType::Sampler;
		m_samplerHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.type = DescriptorHeapType::RTV;
		m_rtvHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.type = DescriptorHeapType::DSV;
		m_dsvHeap.Initialize(m_pDevice, heapDesc);

		for (dU32 i = 0; i < 3; i++)
		{
			Graphics::Descriptor rtv = m_rtvHeap.Allocate();
			m_pDevice->CreateRTV(rtv, m_swapchain.GetBackBuffer(i), {});
			m_renderTargetsDescriptors[i] = rtv;
		}

		m_depthBufferDescriptor = m_dsvHeap.Allocate();
		m_pDevice->CreateDSV(m_depthBufferDescriptor, m_depthBuffer, {});

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		m_forwardPass.Initialize(m_pDevice);
		m_depthPrepass.Initialize(m_pDevice);
	}

	void Renderer::Destroy()
	{
		for (dU32 i = 0; i < 3; i++) 
		{
			Frame& frame = m_frames[i];
			while (!frame.descriptorsToRelease.empty())
			{
				m_srvHeap.Free(frame.descriptorsToRelease.front());
				frame.descriptorsToRelease.pop();
			}
			while (!frame.buffersToRelease.empty())
			{
				frame.buffersToRelease.front().Destroy();
				frame.buffersToRelease.pop();
			}
			WaitForFrame(frame);
			m_rtvHeap.Free(m_renderTargetsDescriptors[i]);
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
		}
		m_dsvHeap.Free(m_depthBufferDescriptor);
		
		m_srvHeap.Destroy();
		m_samplerHeap.Destroy();
		m_rtvHeap.Destroy();
		m_dsvHeap.Destroy();
		m_barrier.Destroy();
		m_forwardPass.Destroy();
		m_depthPrepass.Destroy();
		m_depthBuffer.Destroy();
		m_commandQueue.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();
	}

	void Renderer::OnResize(dU32 width, dU32 height) 
	{
		for ( const Frame& f : m_frames )
			WaitForFrame(f);
		m_swapchain.Resize(width, height);
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < 3; i++)
			m_pDevice->CreateRTV(m_renderTargetsDescriptors[i], m_swapchain.GetBackBuffer(i), {});

		m_depthBuffer.Destroy();
		m_depthBuffer.Initialize(
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
		m_pDevice->CreateDSV(m_depthBufferDescriptor, m_depthBuffer, {});
	}

	void Renderer::WaitForFrame(const Frame& frame)
	{
		const dU64 fenceValue{ frame.fenceValue };
		if ( m_fence.GetValue() < fenceValue )
			m_fence.Wait(fenceValue);
	}

	void Renderer::Render(Scene& scene, Camera& camera)
	{
		Frame& frame = m_frames[m_frameIndex];
		WaitForFrame(frame);
		while ( !frame.descriptorsToRelease.empty() )
		{
			m_srvHeap.Free(frame.descriptorsToRelease.front());
			frame.descriptorsToRelease.pop();
		}
		while (!frame.buffersToRelease.empty())
		{
			frame.buffersToRelease.front().Destroy();
			frame.buffersToRelease.pop();
		}

		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(m_srvHeap, m_samplerHeap);

		Buffer directionalLights{};
		{
			auto view = scene.registry.view<const DirectionalLight>();
			dU32 directionalLightCount = (dU32)view.size();
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
		{
			auto view = scene.registry.view<const PointLight>();
			dU32 pointLightCount = (dU32)view.size();
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

		m_barrier.PushTransition(m_swapchain.GetBackBuffer(m_frameIndex).Get(), EResourceState::Present, EResourceState::RenderTarget);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();

		Descriptor rtv = m_renderTargetsDescriptors[m_frameIndex];
		Descriptor dsv = m_depthBufferDescriptor;

		const float color[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		frame.commandList.ClearRenderTargetView(rtv, color);
		frame.commandList.ClearDepthBuffer(dsv, m_depthBuffer.GetClearValue()[0], 0);

		Viewport viewport{ 0.0, 0.0, (float)m_pWindow->GetWidth(), (float)m_pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, m_pWindow->GetWidth(), m_pWindow->GetHeight() };
		frame.commandList.SetViewports(1, &viewport);
		frame.commandList.SetScissors(1, &scissor);
		
		frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
		m_depthPrepass.Render(scene, frame.commandList, camera);

		frame.commandList.SetRenderTarget(&rtv.cpuAddress, 1, &dsv.cpuAddress);
		m_forwardPass.Render(scene, m_srvHeap, frame.commandList, camera, directionalLights, pointLights, frame.descriptorsToRelease);

		if (m_pImGui)
			m_pImGui->Render(frame.commandList);

		m_barrier.PushTransition(m_swapchain.GetBackBuffer(m_frameIndex).Get(), EResourceState::RenderTarget, EResourceState::Present);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();

		frame.commandList.Close();
		m_commandQueue.ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_commandQueue.Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % 3;
	}
}
