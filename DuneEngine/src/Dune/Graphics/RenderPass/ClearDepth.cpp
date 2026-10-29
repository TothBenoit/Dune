#include "pch.h"
#include "Dune/Graphics/RenderPass/ClearDepth.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
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
		const RGTextureDSV depthDSV = builder.WriteDSV(context.blackboard.Get<SceneResourceOutputs>().depthBuffer);

		builder.OnExecute([depthDSV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;

				RHI::Descriptor dsv = context.GetDescriptor(depthDSV);
				commandList.ClearDepthBuffer(dsv, context.GetRenderGraph().GetTexture(depthDSV.texture).GetClearValue()[0], 0);
			});
	}

	void ClearDepth::Destroy(Renderer& renderer, void* pData)
	{
	}
}
