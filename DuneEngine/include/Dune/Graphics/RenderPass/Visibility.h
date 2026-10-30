#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"

namespace Dune::Graphics
{
	struct VisibilityData
	{
		dVector<LightData>  lights;
		dVector<dU32>       shadowCasters;
		dVector<dMatrix4x4> shadowMatrices;
		dVector<dU32>       opaqueDraws;
		dVector<dU32>       blendDraws;
	};

	class Visibility
	{
	public:
		static void Compute(RenderPassContext& context, VisibilityData& data);
	private:
		Visibility() = delete;
	};
}
