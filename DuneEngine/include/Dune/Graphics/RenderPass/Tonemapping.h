#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{
	class Renderer;
	class CommandList;
	class Tonemapping
	{
	public:
		void Initialize(Renderer& renderer);
		void Destroy();

		void Render(Renderer& renderer, CommandList& commandList, Descriptor& hdrTargetSRV);

		inline static float minLogLuminance{ -5.0f };
		inline static float maxLogLuminance{ 24.0f };
		inline static float tau{ 1.0f };
	private:

		RootSignature m_averageRS;
		PipelineState m_averagePSO;
		RootSignature m_histogramRS;
		PipelineState m_histogramPSO;
		Descriptor m_histogramUAV;
		Buffer m_histogramBuffer;
		Buffer m_luminanceBuffer;

		RootSignature m_tonemapRS;
		PipelineState m_tonemapPSO;

		PersistentDescriptorHeap m_heap;

		Barrier m_barrier;
	};
}
