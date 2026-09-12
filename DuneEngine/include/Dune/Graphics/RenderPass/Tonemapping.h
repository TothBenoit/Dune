#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/RHI/PSOCache.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{
	class Renderer;
	struct RenderPassContext;

	struct TonemappingData
	{
		float minLogLuminance{ -5.0f };
		float maxLogLuminance{ 24.0f };
		float tau{ 1.0f };

		PSOHandle averagePSO;
		PSOHandle histogramPSO;
		Descriptor histogramUAV;
		Buffer histogramBuffer;
		Buffer luminanceBuffer;

		PSOHandle tonemapPSO;
	};

	class Tonemapping
	{
	public:
		static TonemappingData* Create(Renderer& renderer);
		static void             Setup(RenderGraphBuilder& builder, RenderPassContext& context, TonemappingData* pData);
		static void             Execute(RenderPassContext& context, TonemappingData* pData);
		static void             Destroy(Renderer& renderer, TonemappingData* pData);
	private:
		Tonemapping() = delete;
	};
}
