#pragma once

#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/PSOCache.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/UploadContext.h"

namespace Dune::Graphics
{
	class Renderer;
	class Window;

	class RenderContext
	{
	public:
		void Initialize();
		void Destroy();

		void InitializeRenderer(Renderer& renderer, Window& window);

		[[nodiscard]] Device& GetDevice() { return m_device; }
		[[nodiscard]] CommandQueue& GetCommandQueue() { return m_commandQueue; }
		[[nodiscard]] PSOCache& GetPSOCache() { return m_psoCache; }
		[[nodiscard]] ResourceManager& GetResourceManager() { return m_resourceManager; }
		[[nodiscard]] UploadContext& GetUploadContext() { return m_uploadContext; }

	private:
		Device m_device{};
		CommandQueue m_commandQueue{};
		PSOCache m_psoCache{};
		ResourceManager m_resourceManager{};
		UploadContext m_uploadContext{};
	};
}
