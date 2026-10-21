#include "pch.h"
#include "Dune/Graphics/RenderPass/ClearDepth.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void* ClearDepth::Create(Renderer& renderer)
	{
		return nullptr;
	}

	void ClearDepth::Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData)
	{
		builder.Write(context.pRenderer->GetDepthBufferHandle(), RHI::EResourceState::DepthStencil);
	}

	void ClearDepth::Execute(RenderPassContext& context, void* pData)
	{
		Renderer& renderer = *context.pRenderer;
		RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;

		RHI::Descriptor dsv = renderer.GetDepthBufferDSV();
		commandList.ClearDepthBuffer(dsv, renderer.GetDepthBuffer().GetClearValue()[0], 0);
	}

	void ClearDepth::Destroy(Renderer& renderer, void* pData)
	{
	}
}
