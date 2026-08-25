#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	namespace Graphics
	{
		struct FrameData;
		struct ForwardGlobals;
		class CommandList;
		class Renderer;

		class Forward
		{
		public:
			void Initialize(Device& device);
			void Destroy();

			void Render(FrameData& scene, Renderer& renderer, CommandList& commandList, ForwardGlobals& globals);

		private:
			RootSignature m_forwardRS;
			PipelineState m_forwardPSO;
		};
	}
}
