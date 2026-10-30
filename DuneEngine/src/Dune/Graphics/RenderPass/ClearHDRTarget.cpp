#include "pch.h"
#include "Dune/Graphics/RenderPass/ClearHDRTarget.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void* ClearHDRTarget::Create(Renderer& renderer)
	{
		return nullptr;
	}

	void ClearHDRTarget::Setup(RenderPassContext& context, void* pData)
	{
		RenderGraphBuilder& builder = context.AddPass();
		const RGTextureRTV hdrRTV = builder.WriteRTV(context.blackboard.Get<SceneResourceOutputs>().hdrTarget);

		builder.OnExecute([hdrRTV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;
				commandList.ClearRenderTargetView(context.GetDescriptor(hdrRTV), context.GetRenderGraph().GetTexture(hdrRTV.texture).GetClearValue());
			});
	}

	void ClearHDRTarget::Destroy(Renderer& renderer, void* pData)
	{
	}
}
