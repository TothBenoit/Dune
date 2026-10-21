#include "pch.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	PSO* RenderPassContext::GetPSO(MaterialKey key) const
	{
		return pPass->psos[key];
	}

	PSO* RenderPassContext::GetPipeline(dU32 index) const
	{
		return pPass->pipelines[index];
	}

	dU32 RenderPassContext::GetBindlessIndex(RHI::Descriptor persistentSRV) const
	{ 
		return pRenderer->GetSRVHeap().GetIndex(persistentSRV) + pFrameData->sharedSRVHeapCapacity; 
	}

	RHI::Descriptor RenderPassContext::GetGPUDescriptor(const Frame& frame, RHI::Descriptor persistentSRV) const
	{ 
		return frame.srvHeap.GetDescriptorAt(GetBindlessIndex(persistentSRV)); 
	}
}
