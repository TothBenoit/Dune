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
		RGBuffer buffer;
		dU32 bufferIndex;
	};

	struct LightOutputs 
	{ 
		RGBuffer buffer; 
		dU32 bufferIndex; 
		dU32 count;
	};

	struct ShadowOutputs 
	{ 
		dSpan<const RGTexture> shadows; 
		RGBuffer matrices; 
		dU32 shadowStartIndex;
		dU32 matricesIndex; 
	};
}
