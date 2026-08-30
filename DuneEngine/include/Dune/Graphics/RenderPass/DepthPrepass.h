#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct DepthPrepassData
	{
		RootSignature depthRS;
		PipelineState depthPSO;
		ResourceHandle depthHandle;
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
