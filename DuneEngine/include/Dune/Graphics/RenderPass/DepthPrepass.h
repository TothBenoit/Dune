#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		struct FrameData;
		class CommandList;
		class ResourceManager;
		class DepthPrepass
		{
		public:
			void Initialize(Device& device);
			void Destroy();

			void Render(FrameData& frameData, ResourceManager& resourceManager, CommandList& commandList, const dMatrix4x4& viewProjection);

		private:
			RootSignature m_depthRS;
			PipelineState m_depthPSO;
		};
	}
}
