#pragma once

#include "Dune/Graphics/RHI/PSOCache.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	struct DepthPrepassData
	{
		PSOHandle depthPSO[Material::kDepthVariantCount];
	};

	class DepthPrepass
	{
	public:
		static DepthPrepassData* Create(Renderer& renderer);
		static void              Setup(RenderGraphBuilder& builder, RenderPassContext& context, DepthPrepassData* pData);
		static void              Execute(RenderPassContext& context, DepthPrepassData* pData);
		static void              Destroy(Renderer& renderer, DepthPrepassData* pData);
	private:
		DepthPrepass() = delete;
	};
}
