#include "pch.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void RenderContext::Initialize(const ResourceCache& resourceCache, dU32 physicalDeviceIndex)
	{
		Assert(physicalDeviceIndex < RHI::PhysicalDevice::GetPhysicalDeviceCount());
		Assert(RHI::PhysicalDevice::GetPhysicalDeviceDesc(physicalDeviceIndex).isSupported);
		m_physicalDevice.Initialize(physicalDeviceIndex);
		m_device.Initialize(m_physicalDevice);
		m_commandQueue.Initialize(m_device, RHI::ECommandType::Direct);
		m_srvHeap.Initialize(m_device, { .type = RHI::EDescriptorHeapType::SRV_CBV_UAV, .capacity = kSRVCapacity, .isShaderVisible = true });
		m_srvAllocator.Initialize(m_srvHeap, 0, kSRVCapacity);
		m_psoCache.Initialize(m_device);
		m_resourceManager.Initialize(m_device, resourceCache, m_srvAllocator);
		m_uploadContext.Initialize(m_device, m_commandQueue, {});
		m_fence.Initialize(m_device, 0);
	}

	void RenderContext::Destroy()
	{
		m_uploadContext.Destroy();
		m_fence.Destroy();
		m_resourceManager.Destroy();
		m_psoCache.Destroy();
		m_srvAllocator.Destroy();
		m_srvHeap.Destroy();
		m_commandQueue.Destroy();
		m_device.Destroy();
		m_physicalDevice.Destroy();
	}

	void RenderContext::Signal()
	{
		m_commandQueue.Signal(m_fence, ++m_fenceValue);
	}

	void RenderContext::InitializeRenderer(Renderer& renderer, Window& window, const RendererDesc& desc)
	{
		renderer.Initialize(m_device, m_commandQueue, m_psoCache, m_srvAllocator, window, desc);
	}
}
