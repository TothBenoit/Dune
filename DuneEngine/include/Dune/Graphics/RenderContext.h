#pragma once

#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/PSOCache.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		struct FrameData;

		class RenderContext
		{
		public:
			void Initialize();
			void Destroy();
			void GatherFrameData(const Scene& scene, FrameData& frameData);

			[[nodiscard]] Device& GetDevice() { return m_device; }
			[[nodiscard]] PSOCache& GetPSOCache() { return m_psoCache; }
			[[nodiscard]] ResourceManager& GetResourceManager() { return m_resourceManager; }

		private:
			Device m_device{};
			PSOCache m_psoCache{};
			ResourceManager m_resourceManager{};
		};
	}
}
