#include "pch.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	PSO* RenderPassContext::GetPSO(MaterialKey key) const
	{
		return pRenderer->GetPassPSOs(passTypeIndex).materials[key];
	}

	PSO* RenderPassContext::GetPipeline(dU32 index) const
	{
		return pRenderer->GetPassPSOs(passTypeIndex).pipelines[index];
	}

	dU32 RenderPassContext::GetBindlessIndex(RHI::Descriptor srv) const
	{
		return pRenderer->GetSRVHeap().GetIndex(srv);
	}

	RenderGraphBuilder& RenderPassContext::AddPass()
	{
		return GetRenderGraph().AddPass(passTypeIndex);
	}

	RenderGraph& RenderPassContext::GetRenderGraph() const
	{
		return pRenderer->GetRenderGraph();
	}
}
