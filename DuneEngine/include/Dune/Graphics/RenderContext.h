#pragma once

#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::Graphics
{
	class RenderContext
	{
	public:
		void Initialize();
		void Destroy();

		[[nodiscard]] Device& GetDevice() { return m_device; }
		[[nodiscard]] ResourceManager& GetResourceManager() { return m_resourceManager; }

	private:
		Device m_device{};
		ResourceManager m_resourceManager{};
	};
}
