#pragma once

#include "Dune/Graphics/RHI/GraphicsPipeline.h"

namespace Dune
{
	struct Scene;
	struct Camera;

	namespace Graphics
	{
		class DescriptorHeap;
		struct Descriptor;
		struct ForwardGlobals;
		class CommandList;
		class Buffer;

		class Forward
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, DescriptorHeap& srvHeap, CommandList& commandList, ForwardGlobals& globals, dQueue<Descriptor>& descriptorsToRelease);

		private:
			GraphicsPipeline m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}