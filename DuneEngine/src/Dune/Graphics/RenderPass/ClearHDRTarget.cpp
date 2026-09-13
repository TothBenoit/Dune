#include "pch.h"
#include "Dune/Graphics/RenderPass/ClearHDRTarget.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void* ClearHDRTarget::Create(Renderer& renderer)
	{
		return nullptr;
	}

	void ClearHDRTarget::Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData)
	{
		builder.Write(context.pRenderer->GetHDRTargetHandle(), EResourceState::RenderTarget);
	}

	void ClearHDRTarget::Execute(RenderPassContext& context, void* pData)
	{
		Renderer& renderer = *context.pRenderer;
		CommandList& commandList = renderer.GetCurrentFrame().commandList;
		commandList.ClearRenderTargetView(renderer.GetHDRTargetRTV(), renderer.GetHDRTarget().GetClearValue());
	}

	void ClearHDRTarget::Destroy(Renderer& renderer, void* pData)
	{
	}
}
