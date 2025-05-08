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
		class CommandList;
		class Buffer;

		class Forward
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, DescriptorHeap& srvHeap, CommandList& commandList, Camera& camera, Buffer& directionLights, Buffer& pointLights, dQueue<Descriptor>& descriptorsToRelease);

		private:
			GraphicsPipeline m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}