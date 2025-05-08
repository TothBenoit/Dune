#pragma once

#include "Dune/Graphics/RHI/GraphicsPipeline.h"

namespace Dune
{
	struct Scene;
	struct Camera;

	namespace Graphics
	{
		class CommandList;
		class DepthPrepass
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, CommandList& commandList, Camera& camera);

		private:
			GraphicsPipeline m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}