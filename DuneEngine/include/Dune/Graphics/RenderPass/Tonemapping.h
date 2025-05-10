#pragma once

#include "Dune/Graphics/RHI/GraphicsPipeline.h"

namespace Dune::Graphics
{
	class CommandList;
	struct Descriptor;
	class Tonemapping
	{
	public:
		void Initialize(Device* pDevice);
		void Destroy();

		void Render(CommandList& commandList, Descriptor& source);

	private:
		GraphicsPipeline m_pipeline;
		Device* m_pDevice{ nullptr };
	};
}
