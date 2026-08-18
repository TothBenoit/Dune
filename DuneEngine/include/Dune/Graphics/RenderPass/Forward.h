#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	class Scene;
	struct Camera;

	namespace Graphics
	{
		struct Descriptor;
		struct ForwardGlobals;
		class CommandList;
		class Buffer;
		class Renderer;

		class Forward
		{
		public:
			void Initialize(Device& device);
			void Destroy();

			void Render(Scene& scene, Renderer& renderer, CommandList& commandList, ForwardGlobals& globals);

		private:
			RootSignature m_forwardRS;
			PipelineState m_forwardPSO;
		};
	}
}
