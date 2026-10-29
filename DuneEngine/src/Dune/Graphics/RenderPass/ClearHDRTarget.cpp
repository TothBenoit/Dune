#include "pch.h"
#include "Dune/Graphics/RenderPass/ClearHDRTarget.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void* ClearHDRTarget::Create(Renderer& renderer)
	{
		return nullptr;
	}

	void ClearHDRTarget::Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData)
	{
		const RGTextureRTV hdrRTV = builder.WriteRTV(context.pRenderer->GetHDRTargetHandle());

		builder.OnExecute([hdrRTV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;
				commandList.ClearRenderTargetView(context.GetDescriptor(hdrRTV), renderer.GetHDRTarget().GetClearValue());
			});
	}

	void ClearHDRTarget::Destroy(Renderer& renderer, void* pData)
	{
	}
}
