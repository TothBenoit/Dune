#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;
	class RenderGraphBuilder;
	struct RenderPassContext;

	struct ForwardData
	{
		RootSignature forwardRS;
		PipelineState forwardPSO[Material::kVariantCount];
	};

	class Forward
	{
	public:
		static ForwardData* Create(Renderer& renderer);
		static void         Setup(RenderGraphBuilder& builder, RenderPassContext& context, ForwardData* pData);
		static void         Execute(RenderPassContext& context, ForwardData* pData);
		static void         Destroy(Renderer& renderer, ForwardData* pData);
	private:
		Forward() = delete;
	};
}
