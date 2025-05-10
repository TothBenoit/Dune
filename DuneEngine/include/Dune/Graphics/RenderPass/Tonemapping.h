#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

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
		RootSignature m_rootSignature;
		PipelineState m_pipeline;
		Device* m_pDevice{ nullptr };
	};
}
