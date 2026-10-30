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
		RHI::Device& device = *renderer.GetDevice();

		TonemappingData* pData = new TonemappingData();

		pData->histogramBuffer.Initialize(device, { .debugName = L"HistogramBuffer", .usage = RHI::EBufferUsage::UAV, .memory = RHI::EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32)});
		pData->histogramCPUUAV = renderer.GetCPUSRVAllocator().Allocate();
		device.CreateUAV(pData->histogramCPUUAV, pData->histogramBuffer, { .format = RHI::EFormat::R32_UINT, .elementCount = 256 });
		pData->histogramUAV = renderer.GetSRVAllocator().Allocate();
		device.CopyDescriptors(1, pData->histogramCPUUAV.cpuAddress, pData->histogramUAV.cpuAddress, RHI::EDescriptorHeapType::SRV_CBV_UAV);

		pData->luminanceBuffer.Initialize(device, { .debugName = L"LuminanceBuffer", .usage = RHI::EBufferUsage::UAV, .memory = RHI::EBufferMemory::GPU, .byteSize = sizeof(dU32)});

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
		RenderGraphBuilder& builder = context.AddPass();
		const RGTextureSRV hdrSRV = builder.ReadSRV(context.blackboard.Get<SceneResourceOutputs>().hdrTarget);
		const RGTextureRTV backBufferRTV = builder.WriteRTV(renderer.GetBackBufferHandle());

		builder.OnExecute([pData, hdrSRV, backBufferRTV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				RHI::CommandList& commandList = frame.commandList;
				const dUVec2 resolution = renderer.GetResolution();
				RHI::Barrier& barrier = renderer.GetBarrier();
				PSO& histogramPSO = *context.GetPipeline(Tonemapping::kHistogramPipeline);
				PSO& averagePSO = *context.GetPipeline(Tonemapping::kAveragePipeline);
				PSO& tonemapPSO = *context.GetPipeline(Tonemapping::kTonemapPipeline);

				const RHI::Descriptor rtv = context.GetDescriptor(backBufferRTV);
				commandList.SetRenderTarget(&rtv.cpuAddress, 1, nullptr);

				commandList.ClearUAVUInt(pData->histogramUAV.gpuAddress, pData->histogramCPUUAV.cpuAddress, pData->histogramBuffer.Get(), 0);

				float logLuminanceRange = pData->maxLogLuminance - pData->minLogLuminance;
				LuminanceHistogramParams histogramParams
				{
					.width = resolution.x,
					.height = resolution.y,
					.minLogLuminance = pData->minLogLuminance,
					.oneOverLogLuminanceRange = 1.0f / logLuminanceRange,
				};

				commandList.SetComputeRootSignature(*histogramPSO.pRootSignature);
				commandList.SetPipelineState(histogramPSO.pipelineState);
				commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
				RHI::Descriptor hdrTargetSRV = context.GetDescriptor(hdrSRV);
				commandList.BindComputeGroup(1, hdrTargetSRV);
				commandList.PushComputeUAV(2, pData->histogramBuffer);
				commandList.Dispatch((histogramParams.width + 16 - 1) / 16, (histogramParams.height + 16 - 1) / 16, 1);

				barrier.PushTransition(pData->histogramBuffer, RHI::EResourceState::UAV, RHI::EResourceState::ShaderResource);
				commandList.Transition(barrier);
				barrier.Reset();

				LuminanceAverageParams averageParams
				{
					.pixelCount = histogramParams.width * histogramParams.height,
					.minLogLuminance = pData->minLogLuminance,
					.logLuminanceRange = logLuminanceRange,
					.timeDelta = context.pFrameData->deltaTime,
					.tau = pData->tau
				};

				commandList.SetComputeRootSignature(*averagePSO.pRootSignature);
				commandList.SetPipelineState(averagePSO.pipelineState);
				commandList.PushComputeConstants(0, &averageParams, sizeof(averageParams));
				commandList.PushComputeSRV(1, pData->histogramBuffer);
				commandList.PushComputeUAV(2, pData->luminanceBuffer);
				commandList.Dispatch(1, 1, 1);

				barrier.PushTransition(pData->luminanceBuffer, RHI::EResourceState::UAV, RHI::EResourceState::ShaderResource);
				commandList.Transition(barrier);
				barrier.Reset();

				RHI::Viewport viewport{ 0.0, 0.0, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f };
				RHI::Scissor scissor{ 0, 0, resolution.x, resolution.y };
				commandList.SetViewports(1, &viewport);
				commandList.SetScissors(1, &scissor);
				commandList.SetGraphicsRootSignature(*tonemapPSO.pRootSignature);
				commandList.SetPipelineState(tonemapPSO.pipelineState);
				commandList.SetPrimitiveTopology(RHI::EPrimitiveTopology::TriangleList);
				commandList.BindGraphicsGroup(0, hdrTargetSRV);
				commandList.PushGraphicsSRV(1, pData->luminanceBuffer);
				commandList.DrawInstanced(3, 1, 0, 0);
			});
	}

	void Tonemapping::Destroy(Renderer& renderer, TonemappingData* pData)
	{
		renderer.GetSRVAllocator().Free(pData->histogramUAV);
		renderer.GetCPUSRVAllocator().Free(pData->histogramCPUUAV);
		pData->histogramBuffer.Destroy();
		pData->luminanceBuffer.Destroy();
		delete pData;
	}
}
