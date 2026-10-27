#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"

namespace Dune::Graphics
{
	class Renderer;

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
		static VisibilityData* Create(Renderer& renderer);
		static void            Setup(RenderGraphBuilder& builder, RenderPassContext& context, VisibilityData* pData);
		static void            Destroy(Renderer& renderer, VisibilityData* pData);
	private:
		Visibility() = delete;
	};
}
