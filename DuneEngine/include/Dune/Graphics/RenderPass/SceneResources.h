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
		static void Initialize(Renderer& renderer, SceneResourcesData& data);
		static void Setup(RenderPassContext& context, SceneResourcesData& data);
	private:
		SceneResources() = delete;
	};
}
