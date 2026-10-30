#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/Shader.h"
#include "Dune/Platform/RHI/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	TonemappingData* Tonemapping::Create(Renderer& renderer)
	{
		RenderGraph& graph = renderer.GetRenderGraph();

		TonemappingData* pData = new TonemappingData();

		pData->histogramBuffer = graph.CreateBuffer({ .debugName = L"HistogramBuffer", .usage = RHI::EBufferUsage::UAV, .memory = RHI::EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32)});
		pData->luminanceBuffer = graph.CreateBuffer({ .debugName = L"LuminanceBuffer", .usage = RHI::EBufferUsage::UAV, .memory = RHI::EBufferMemory::GPU, .byteSize = sizeof(dU32)});

		return pData;
	}

	dVector<PipelineDesc> Tonemapping::GetPipelineDescs()
	{
		static const RHI::BindingSlot kHistogramLayout[]
		{
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(LuminanceHistogramParams), .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::UAV, .visibility = RHI::EShaderVisibility::All },
		};
		static const RHI::BindingSlot kAverageLayout[]
		{
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(LuminanceAverageParams), .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::SRV, .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::UAV, .visibility = RHI::EShaderVisibility::All },
		};
		static const RHI::BindingSlot kTonemapLayout[]
		{
			{ .type = RHI::EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = RHI::EShaderVisibility::Pixel },
			{ .type = RHI::EBindingType::SRV, .visibility = RHI::EShaderVisibility::Pixel },
		};

		static_assert(kHistogramPipeline == 0 && kAveragePipeline == 1 && kTonemapPipeline == 2 && kPipelineCount == 3);
		return
		{
			PipelineDesc
			{
				.computeShader = { .path = FileSystem::Resolve<RHI::Shader>("engine://Shaders/LuminanceHistogram.hlsl"), .stage = RHI::EShaderStage::Compute },
				.rootSignature = { .layout = kHistogramLayout },
			},
			PipelineDesc
			{
				.computeShader = { .path = FileSystem::Resolve<RHI::Shader>("engine://Shaders/LuminanceAverage.hlsl"), .stage = RHI::EShaderStage::Compute },
				.rootSignature = { .layout = kAverageLayout },
			},
			PipelineDesc
			{
				.vertexShader = { .path = FileSystem::Resolve<RHI::Shader>("engine://Shaders/FullScreenTriangle.hlsl"), .stage = RHI::EShaderStage::Vertex },
				.pixelShader = { .path = FileSystem::Resolve<RHI::Shader>("engine://Shaders/Tonemapping.hlsl"), .stage = RHI::EShaderStage::Pixel },
				.rootSignature = { .layout = kTonemapLayout },
				.renderTargetCount = 1,
				.renderTargetsFormat = { RHI::EFormat::R8G8B8A8_UNORM },
			},
		};
	}

	void Tonemapping::Setup(RenderPassContext& context, TonemappingData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		const RGTexture hdrTarget = context.blackboard.Get<SceneResourceOutputs>().hdrTarget;
		const RGBuffer histogramBuffer = pData->histogramBuffer;
		const RGBuffer luminanceBuffer = pData->luminanceBuffer;

		RenderGraphBuilder& clearBuilder = context.AddPass();
		clearBuilder.Write(histogramBuffer, RHI::EResourceState::CopyDest);
		clearBuilder.OnExecute([histogramBuffer](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::Buffer& histogram = context.GetRenderGraph().GetBuffer(histogramBuffer);
				const dU32 histogramByteSize = histogram.GetByteSize();
				StagingMemory staging = renderer.AllocateStaging(histogramByteSize, 16);
				memset(staging.pCpuAddress, 0, histogramByteSize);
				renderer.GetCurrentFrame().commandList.CopyBufferRegion(histogram, 0, staging.buffer, staging.offset, histogramByteSize);
			});

		RenderGraphBuilder& histogramBuilder = context.AddPass();
		const RGTextureSRV histogramHdrSRV = histogramBuilder.ReadSRV(hdrTarget);
		histogramBuilder.Write(histogramBuffer, RHI::EResourceState::UAV);
		histogramBuilder.OnExecute([pData, histogramHdrSRV, histogramBuffer](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;
				const dUVec2 resolution = renderer.GetResolution();
				PSO& histogramPSO = *context.GetPipeline(Tonemapping::kHistogramPipeline);

				LuminanceHistogramParams histogramParams
				{
					.width = resolution.x,
					.height = resolution.y,
					.minLogLuminance = pData->minLogLuminance,
					.oneOverLogLuminanceRange = 1.0f / (pData->maxLogLuminance - pData->minLogLuminance),
				};

				commandList.SetComputeRootSignature(*histogramPSO.pRootSignature);
				commandList.SetPipelineState(histogramPSO.pipelineState);
				commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
				commandList.BindComputeGroup(1, context.GetDescriptor(histogramHdrSRV));
				commandList.PushComputeUAV(2, context.GetRenderGraph().GetBuffer(histogramBuffer));
				commandList.Dispatch((histogramParams.width + 16 - 1) / 16, (histogramParams.height + 16 - 1) / 16, 1);
			});

		RenderGraphBuilder& averageBuilder = context.AddPass();
		averageBuilder.Read(histogramBuffer, RHI::EResourceState::ShaderResource);
		averageBuilder.Write(luminanceBuffer, RHI::EResourceState::UAV);
		averageBuilder.OnExecute([pData, histogramBuffer, luminanceBuffer](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;
				const dUVec2 resolution = renderer.GetResolution();
				RenderGraph& graph = context.GetRenderGraph();
				PSO& averagePSO = *context.GetPipeline(Tonemapping::kAveragePipeline);

				LuminanceAverageParams averageParams
				{
					.pixelCount = resolution.x * resolution.y,
					.minLogLuminance = pData->minLogLuminance,
					.logLuminanceRange = pData->maxLogLuminance - pData->minLogLuminance,
					.timeDelta = context.pFrameData->deltaTime,
					.tau = pData->tau
				};

				commandList.SetComputeRootSignature(*averagePSO.pRootSignature);
				commandList.SetPipelineState(averagePSO.pipelineState);
				commandList.PushComputeConstants(0, &averageParams, sizeof(averageParams));
				commandList.PushComputeSRV(1, graph.GetBuffer(histogramBuffer));
				commandList.PushComputeUAV(2, graph.GetBuffer(luminanceBuffer));
				commandList.Dispatch(1, 1, 1);
			});

		RenderGraphBuilder& tonemapBuilder = context.AddPass();
		const RGTextureSRV tonemapHdrSRV = tonemapBuilder.ReadSRV(hdrTarget);
		tonemapBuilder.Read(luminanceBuffer, RHI::EResourceState::ShaderResource);
		const RGTextureRTV backBufferRTV = tonemapBuilder.WriteRTV(renderer.GetBackBufferHandle());
		tonemapBuilder.OnExecute([tonemapHdrSRV, luminanceBuffer, backBufferRTV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				RHI::CommandList& commandList = renderer.GetCurrentFrame().commandList;
				const dUVec2 resolution = renderer.GetResolution();
				PSO& tonemapPSO = *context.GetPipeline(Tonemapping::kTonemapPipeline);

				const RHI::Descriptor rtv = context.GetDescriptor(backBufferRTV);
				commandList.SetRenderTarget(&rtv.cpuAddress, 1, nullptr);

				RHI::Viewport viewport{ 0.0, 0.0, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f };
				RHI::Scissor scissor{ 0, 0, resolution.x, resolution.y };
				commandList.SetViewports(1, &viewport);
				commandList.SetScissors(1, &scissor);
				commandList.SetGraphicsRootSignature(*tonemapPSO.pRootSignature);
				commandList.SetPipelineState(tonemapPSO.pipelineState);
				commandList.SetPrimitiveTopology(RHI::EPrimitiveTopology::TriangleList);
				commandList.BindGraphicsGroup(0, context.GetDescriptor(tonemapHdrSRV));
				commandList.PushGraphicsSRV(1, context.GetRenderGraph().GetBuffer(luminanceBuffer));
				commandList.DrawInstanced(3, 1, 0, 0);
			});
	}

	void Tonemapping::Destroy(Renderer&, TonemappingData* pData)
	{
		delete pData;
	}
}
