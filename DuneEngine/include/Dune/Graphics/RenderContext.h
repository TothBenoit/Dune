#pragma once

#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/PhysicalDevice.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/Fence.h"
#include "Dune/Graphics/PSOCache.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/UploadContext.h"

namespace Dune
{
	class Window;
}

namespace Dune::Graphics
{
	class Renderer;

	class RenderContext
	{
	public:
		void Initialize(const ResourceHandles& handles, dU32 physicalDeviceIndex);
		void Destroy();

		void InitializeRenderer(Renderer& renderer, Window& window);

		[[nodiscard]] RHI::Device& GetDevice() { return m_device; }
		[[nodiscard]] const RHI::PhysicalDevice& GetPhysicalDevice() const { return m_physicalDevice; }
		[[nodiscard]] RHI::CommandQueue& GetCommandQueue() { return m_commandQueue; }
		[[nodiscard]] PSOCache& GetPSOCache() { return m_psoCache; }
		[[nodiscard]] ResourceManager& GetResourceManager() { return m_resourceManager; }
		[[nodiscard]] const ResourceManager& GetResourceManager() const { return m_resourceManager; }
		[[nodiscard]] UploadContext& GetUploadContext() { return m_uploadContext; }

		void Signal();
		[[nodiscard]] dU64 GetNextFenceValue() const { return m_fenceValue + 1; }
		[[nodiscard]] dU64 GetCompletedFenceValue() { return m_fence.GetValue(); }

	private:
		RHI::PhysicalDevice m_physicalDevice{};
		RHI::Device m_device{};
		RHI::CommandQueue m_commandQueue{};
		PSOCache m_psoCache{};
		ResourceManager m_resourceManager{};
		UploadContext m_uploadContext{};
		RHI::Fence m_fence{};
		dU64 m_fenceValue{ 0 };
	};
}
