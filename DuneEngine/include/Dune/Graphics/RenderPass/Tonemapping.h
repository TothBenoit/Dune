#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
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

		RootSignature averageRS;
		PipelineState averagePSO;
		RootSignature histogramRS;
		PipelineState histogramPSO;
		Descriptor histogramUAV;
		Buffer histogramBuffer;
		Buffer luminanceBuffer;

		RootSignature tonemapRS;
		PipelineState tonemapPSO;
	};

	class Tonemapping
	{
	public:
		static RenderPassDesc   GetDesc();
		static TonemappingData* Create(Renderer& renderer);
		static void             Execute(RenderPassContext& context, TonemappingData* pData);
		static void             Destroy(Renderer& renderer, TonemappingData* pData);
	private:
		Tonemapping() = delete;
	};
}
