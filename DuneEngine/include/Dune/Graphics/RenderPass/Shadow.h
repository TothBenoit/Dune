#pragma once

#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Pipeline.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	struct ShadowPassData
	{
		dVector<RGTexture> shadowHandles;
		dVector<RGTexture> cubeShadowHandles;
		dVector<RGTexture> activeHandles;
		dVector<ShadowData> shadows;

		RGBuffer shadowsHandle;
	};

	class Shadow
	{
	public:
		static ShadowPassData* Create(Renderer& renderer);
		static bool            IsPassMaterial(MaterialKey key);
		static PipelineDesc    GetPipelineDesc(MaterialKey key);
		static void            Setup(RenderPassContext& context, ShadowPassData* pData);
		static void            Destroy(Renderer& renderer, ShadowPassData* pData);
	private:
		Shadow() = delete;
	};
}
