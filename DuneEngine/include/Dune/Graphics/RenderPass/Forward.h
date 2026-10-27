#pragma once

#include "Dune/Graphics/Pipeline.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;
	class RenderGraphBuilder;
	struct RenderPassContext;

	class Forward
	{
	public:
		static void*        Create(Renderer& renderer);
		static PipelineDesc GetPipelineDesc(MaterialKey key);
		static void         Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData);
		static void         Destroy(Renderer& renderer, void* pData);
	private:
		Forward() = delete;
	};
}
