#pragma once
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	struct MaterialOutputs
	{
		ResourceHandle buffer;
		dU32 bufferIndex;
	};

	struct LightOutputs 
	{ 
		ResourceHandle buffer; 
		dU32 bufferIndex; 
		dU32 count;
	};

	struct ShadowOutputs 
	{ 
		dSpan<const ResourceHandle> shadows; 
		ResourceHandle matrices; 
		dU32 shadowStartIndex;
		dU32 matricesIndex; 
	};
}
