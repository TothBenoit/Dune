#include "pch.h"
#include "Dune/Graphics/RenderPass/SceneResources.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	SceneResourcesData* SceneResources::Create(Renderer& renderer)
	{
		RenderGraph& renderGraph = renderer.GetRenderGraph();
		SceneResourcesData* pData = new SceneResourcesData();
		pData->resolution = renderer.GetResolution();
		const dU32 width = std::max(1u, pData->resolution.x);
		const dU32 height = std::max(1u, pData->resolution.y);

		pData->hdrTarget = renderGraph.CreateTexture(
			{
				.debugName = L"HDRTarget",
				.usage = RHI::ETextureUsage::RenderTarget | RHI::ETextureUsage::ShaderResource,
				.width = width,
				.height = height,
				.mipLevels = 1,
				.format = RHI::EFormat::R16G16B16A16_FLOAT,
				.initialState = RHI::EResourceState::ShaderResource,
			});

		pData->depthBuffer = renderGraph.CreateTexture(
			{
				.debugName = L"DepthBuffer",
				.usage = RHI::ETextureUsage::DepthStencil,
				.width = width,
				.height = height,
				.format = RHI::EFormat::D32_FLOAT,
				.clearValue = { 1.f, 1.f, 1.f, 1.f },
				.initialState = RHI::EResourceState::DepthStencil,
			});

		return pData;
	}

	void SceneResources::Setup(RenderPassContext& context, SceneResourcesData* pData)
	{
		const dUVec2 resolution = context.pRenderer->GetResolution();
		if (resolution.x != pData->resolution.x || resolution.y != pData->resolution.y)
		{
			RenderGraph& renderGraph = context.GetRenderGraph();
			const dU32 width = std::max(1u, resolution.x);
			const dU32 height = std::max(1u, resolution.y);
			renderGraph.ResizeTexture(pData->hdrTarget, width, height);
			renderGraph.ResizeTexture(pData->depthBuffer, width, height);
			pData->resolution = resolution;
		}

		context.blackboard.Add<SceneResourceOutputs>({ .hdrTarget = pData->hdrTarget, .depthBuffer = pData->depthBuffer });
	}

	void SceneResources::Destroy(Renderer&, SceneResourcesData* pData)
	{
		delete pData;
	}
}
