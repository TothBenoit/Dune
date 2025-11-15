#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	struct Scene;
	struct Camera;

	namespace Graphics
	{
		class TransientDescriptorHeap;
		struct Descriptor;
		struct ForwardGlobals;
		class CommandList;
		class Buffer;

		class Forward
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, TransientDescriptorHeap& srvHeap, CommandList& commandList, ForwardGlobals& globals);

		private:
			RootSignature m_rootSignature;
			PipelineState m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}