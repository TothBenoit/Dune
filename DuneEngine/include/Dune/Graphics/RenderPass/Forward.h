#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	struct Scene;
	struct Camera;

	namespace Graphics
	{
		class ScratchDescriptorHeap;
		struct Descriptor;
		struct ForwardGlobals;
		class CommandList;
		class Buffer;

		class Forward
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, ScratchDescriptorHeap& srvHeap, CommandList& commandList, ForwardGlobals& globals);

		private:
			RootSignature m_forwardRS;
			PipelineState m_forwardPSO;
			Device* m_pDevice{ nullptr };
		};
	}
}