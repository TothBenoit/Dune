#include "pch.h"
#include "Dune/Graphics/RenderPass/SceneResources.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void SceneResources::Initialize(Renderer& renderer, SceneResourcesData& data)
	{
		RenderGraph& renderGraph = renderer.GetRenderGraph();
		data.resolution = renderer.GetResolution();
		const dU32 width = std::max(1u, data.resolution.x);
		const dU32 height = std::max(1u, data.resolution.y);

		data.hdrTarget = renderGraph.CreateTexture(
			{
				.debugName = L"HDRTarget",
				.usage = RHI::ETextureUsage::RenderTarget | RHI::ETextureUsage::ShaderResource,
				.width = width,
				.height = height,
				.mipLevels = 1,
				.format = RHI::EFormat::R16G16B16A16_FLOAT,
				.initialState = RHI::EResourceState::ShaderResource,
			});

		data.depthBuffer = renderGraph.CreateTexture(
			{
				.debugName = L"DepthBuffer",
				.usage = RHI::ETextureUsage::DepthStencil,
				.width = width,
				.height = height,
				.format = RHI::EFormat::D32_FLOAT,
				.clearValue = { 1.f, 1.f, 1.f, 1.f },
				.initialState = RHI::EResourceState::DepthStencil,
			});
	}

	void SceneResources::Setup(RenderPassContext& context, SceneResourcesData& data)
	{
		const dUVec2 resolution = context.pRenderer->GetResolution();
		if (resolution.x != data.resolution.x || resolution.y != data.resolution.y)
		{
			RenderGraph& renderGraph = context.GetRenderGraph();
			const dU32 width = std::max(1u, resolution.x);
			const dU32 height = std::max(1u, resolution.y);
			renderGraph.ResizeTexture(data.hdrTarget, width, height);
			renderGraph.ResizeTexture(data.depthBuffer, width, height);
			data.resolution = resolution;
		}

		context.blackboard.Add<SceneResourceOutputs>({ .hdrTarget = data.hdrTarget, .depthBuffer = data.depthBuffer });
	}
}
