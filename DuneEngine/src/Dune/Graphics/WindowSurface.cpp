#include "pch.h"
#include "Dune/Graphics/WindowSurface.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Platform/Window.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/Texture.h"

namespace Dune::Graphics
{
	void WindowSurface::Initialize(RenderContext& context, Window& window, const WindowSurfaceDesc& desc)
	{
		RHI::Device& device = context.GetDevice();
		RHI::CommandQueue& commandQueue = context.GetCommandQueue();
		m_pDevice = &device;
		m_pCommandQueue = &commandQueue;
		m_pWindow = &window;
		m_width = window.GetWidth();
		m_height = window.GetHeight();

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(device, RHI::ECommandType::Direct);
			frame.commandList.Initialize(device, RHI::ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
		}
		m_fence.Initialize(device, 0);
		m_swapchain.Initialize(device, &window, &commandQueue, { .latency = kFramesInFlight });
		m_rtvHeap.Initialize(device, { .type = RHI::EDescriptorHeapType::RTV, .capacity = kFramesInFlight, .isShaderVisible = false });
		CreateBackBufferViews();

		if (desc.enableImGui)
			m_imgui.Initialize({ .pWindow = &window, .pDevice = &device, .pCommandQueue = &commandQueue, .framesInFlight = kFramesInFlight, .renderTargetFormat = m_swapchain.GetBackBuffer(0).GetFormat(), .pLayoutPath = desc.pImGuiLayoutPath });
	}

	void WindowSurface::Destroy()
	{
		WaitForFrames();
		if (HasImGui())
			m_imgui.Destroy();
		for (Frame& frame : m_frames)
		{
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
			frame.fenceValue = 0;
		}
		m_rtvHeap.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();
		m_frameCount = 0;
		m_pWindow = nullptr;
	}

	void WindowSurface::Present(dSpan<const SurfaceBlit> blits)
	{
		const dU32 width = m_pWindow->GetWidth();
		const dU32 height = m_pWindow->GetHeight();
		if (width > 0 && height > 0 && (width != m_width || height != m_height))
		{
			WaitForFrames();
			m_swapchain.Resize(width, height);
			m_width = width;
			m_height = height;
			CreateBackBufferViews();
		}

		const dU32 index = m_swapchain.GetCurrentBackBufferIndex();
		Frame& frame = m_frames[index];
		if (m_fence.GetValue() < frame.fenceValue)
			m_fence.Wait(frame.fenceValue);
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);

		RHI::CommandList& commandList = frame.commandList;
		RHI::Texture& backBuffer = m_swapchain.GetBackBuffer(index);
		const RHI::Descriptor rtv = m_rtvHeap.GetDescriptorAt(index);
		const float clearColor[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		commandList.Transition(backBuffer, RHI::EResourceState::Present, RHI::EResourceState::RenderTarget);
		commandList.ClearRenderTargetView(rtv, clearColor);
		commandList.Transition(backBuffer, RHI::EResourceState::RenderTarget, RHI::EResourceState::CopyDest);
		for (const SurfaceBlit& blit : blits)
		{
			const RHI::TextureDesc& desc = blit.pTexture->GetDesc();
			if (blit.x < m_width && blit.y < m_height)
				commandList.CopyTextureRegion(backBuffer, blit.x, blit.y, *blit.pTexture, std::min(desc.width, m_width - blit.x), std::min(desc.height, m_height - blit.y));
		}
		commandList.Transition(backBuffer, RHI::EResourceState::CopyDest, RHI::EResourceState::RenderTarget);
		if (HasImGui())
		{
			commandList.SetRenderTarget(&rtv.cpuAddress, 1, nullptr);
			m_imgui.Render(commandList);
		}
		commandList.Transition(backBuffer, RHI::EResourceState::RenderTarget, RHI::EResourceState::Present);
		commandList.Close();

		m_pCommandQueue->ExecuteCommandLists(&commandList, 1);
		m_swapchain.Present();
		m_pCommandQueue->Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
	}

	void WindowSurface::WaitForFrames()
	{
		for (const Frame& frame : m_frames)
		{
			if (m_fence.GetValue() < frame.fenceValue)
				m_fence.Wait(frame.fenceValue);
		}
	}

	void WindowSurface::CreateBackBufferViews()
	{
		for (dU32 i = 0; i < kFramesInFlight; i++)
			m_pDevice->CreateRTV(m_rtvHeap.GetDescriptorAt(i), m_swapchain.GetBackBuffer(i), {});
	}
}
