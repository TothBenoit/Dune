#pragma once

#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;
	struct RenderPassContext;

	struct SceneResourcesData
	{
		RGTexture hdrTarget;
		RGTexture depthBuffer;
		dUVec2 resolution{ 0, 0 };
	};

	class SceneResources
	{
	public:
		static SceneResourcesData* Create(Renderer& renderer);
		static void                Setup(RenderGraphBuilder& builder, RenderPassContext& context, SceneResourcesData* pData);
		static void                Destroy(Renderer& renderer, SceneResourcesData* pData);
	private:
		SceneResources() = delete;
	};
}
