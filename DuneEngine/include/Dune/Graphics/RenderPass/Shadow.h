#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		class CommandList;
		class ResourceManager;
		class Shadow
		{
		public:
			void Initialize(Device& device);
			void Destroy();

			void Render(Scene& scene, ResourceManager& resourceManager, CommandList& commandList, const dMatrix4x4& viewProjection);

		private:
			RootSignature m_shadowRS;
			PipelineState m_shadowPSO;
		};
	}
}
