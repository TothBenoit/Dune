#include "pch.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	PSO* RenderPassContext::GetPSO(MaterialKey key) const
	{
		return pRenderer->GetPassPSOs(pPass->typeIndex).materials[key];
	}

	PSO* RenderPassContext::GetPipeline(dU32 index) const
	{
		return pRenderer->GetPassPSOs(pPass->typeIndex).pipelines[index];
	}

	dU32 RenderPassContext::GetBindlessIndex(RHI::Descriptor srv) const
	{
		return pRenderer->GetSRVHeap().GetIndex(srv);
	}

	RenderGraph& RenderPassContext::GetRenderGraph() const
	{
		return pRenderer->GetRenderGraph();
	}
}
