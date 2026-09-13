#include "pch.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	dU32 RenderPassContext::GetBindlessIndex(Descriptor persistentSRV) const
	{ 
		return pRenderer->GetSRVHeap().GetIndex(persistentSRV) + pFrameData->sharedSRVHeapCapacity; 
	}

	Descriptor RenderPassContext::GetGPUDescriptor(const Frame& frame, Descriptor persistentSRV) const
	{ 
		return frame.srvHeap.GetDescriptorAt(GetBindlessIndex(persistentSRV)); 
	}
}
