#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Pipeline.h"

namespace Dune::Graphics
{
	class Renderer;
	struct RenderPassContext;

	struct TonemappingData
	{
		float minLogLuminance{ -5.0f };
		float maxLogLuminance{ 24.0f };
		float tau{ 1.0f };

		RGBuffer histogramBuffer;
		RGBuffer luminanceBuffer;
	};

	class Tonemapping
	{
	public:
		static constexpr dU32 kHistogramPipeline = 0;
		static constexpr dU32 kAveragePipeline = 1;
		static constexpr dU32 kTonemapPipeline = 2;
		static constexpr dU32 kPipelineCount = 3;

		static TonemappingData*      Create(Renderer& renderer);
		static dVector<PipelineDesc> GetPipelineDescs();
		static void                  Setup(RenderPassContext& context, TonemappingData* pData);
		static void                  Destroy(Renderer& renderer, TonemappingData* pData);
	private:
		Tonemapping() = delete;
	};
}
