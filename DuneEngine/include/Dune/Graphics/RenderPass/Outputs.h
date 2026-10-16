#pragma once
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"

namespace Dune::Graphics
{
	struct VisibleSet
	{
		dSpan<const LightData>  lights;
		dSpan<const dU32>       shadowCasters;
		dSpan<const dMatrix4x4> shadowMatrices;
		dSpan<const dU32>       opaqueDraws;
		dSpan<const dU32>       blendDraws;
	};

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
