#pragma once

#include "Dune/Graphics/RHI/GraphicsPipeline.h"

namespace Dune
{
	struct Scene;

	namespace Graphics
	{
		class CommandList;
		class Shadow
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, CommandList& commandList, const dMatrix4x4& viewProjection);

		private:
			GraphicsPipeline m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}