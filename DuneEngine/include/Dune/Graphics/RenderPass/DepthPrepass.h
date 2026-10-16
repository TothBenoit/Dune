#pragma once

#include "Dune/Graphics/Pipeline.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	class DepthPrepass
	{
	public:
		static void*        Create(Renderer& renderer);
		static bool         IsPassMaterial(MaterialKey key);
		static PipelineDesc GetPipelineDesc(MaterialKey key);
		static void         Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData);
		static void         Execute(RenderPassContext& context, void* pData);
		static void         Destroy(Renderer& renderer, void* pData);
	private:
		DepthPrepass() = delete;
	};
}
