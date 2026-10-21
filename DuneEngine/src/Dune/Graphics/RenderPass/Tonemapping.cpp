#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Component/Camera.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	void Tonemapping::Setup(RenderGraphBuilder& builder, RenderPassContext& context, TonemappingData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		builder.Read(renderer.GetHDRTargetHandle(), EResourceState::ShaderResource);
		builder.Write(renderer.GetBackBufferHandle(), EResourceState::RenderTarget);
	}

	TonemappingData* Tonemapping::Create(Renderer& renderer)
	{
		Device& device = *renderer.GetDevice();
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();

		TonemappingData* pData = new TonemappingData();

		pData->histogramBuffer.Initialize(device, { .debugName = L"HistogramBuffer", .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32)});
		pData->histogramUAV = srvHeap.Allocate();
		device.CreateUAV(pData->histogramUAV, pData->histogramBuffer, { .format = EFormat::R32_UINT, .elementCount = 256 });

		pData->luminanceBuffer.Initialize(device, { .debugName = L"LuminanceBuffer", .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = sizeof(dU32)});

		return pData;
	}

	dVector<PipelineDesc> Tonemapping::GetPipelineDescs()
	{
		static const BindingSlot kHistogramLayout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(LuminanceHistogramParams), .visibility = EShaderVisibility::All },
			{ .type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::All },
			{ .type = EBindingType::UAV, .visibility = EShaderVisibility::All },
		};
		static const BindingSlot kAverageLayout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(LuminanceAverageParams), .visibility = EShaderVisibility::All },
			{ .type = EBindingType::SRV, .visibility = EShaderVisibility::All },
			{ .type = EBindingType::UAV, .visibility = EShaderVisibility::All },
		};
		static const BindingSlot kTonemapLayout[]
		{
			{ .type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::Pixel },
			{ .type = EBindingType::SRV, .visibility = EShaderVisibility::Pixel },
		};

		static_assert(kHistogramPipeline == 0 && kAveragePipeline == 1 && kTonemapPipeline == 2 && kPipelineCount == 3);
		return
		{
			PipelineDesc
			{
				.computeShader = { .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/LuminanceHistogram.hlsl"), .stage = EShaderStage::Compute },
				.rootSignature = { .layout = kHistogramLayout },
			},
			PipelineDesc
			{
				.computeShader = { .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/LuminanceAverage.hlsl"), .stage = EShaderStage::Compute },
				.rootSignature = { .layout = kAverageLayout },
			},
			PipelineDesc
			{
				.vertexShader = { .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/FullScreenTriangle.hlsl"), .stage = EShaderStage::Vertex },
				.pixelShader = { .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/Tonemapping.hlsl"), .stage = EShaderStage::Pixel },
				.rootSignature = { .layout = kTonemapLayout },
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R8G8B8A8_UNORM },
			},
		};
	}

	void Tonemapping::Execute(RenderPassContext& context, TonemappingData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		Descriptor histogramUAV = frame.srvHeap.Allocate(1);
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();
		Barrier& barrier = renderer.GetBarrier();
		PSO& histogramPSO = *context.GetPipeline(Tonemapping::kHistogramPipeline);
		PSO& averagePSO = *context.GetPipeline(Tonemapping::kAveragePipeline);
		PSO& tonemapPSO = *context.GetPipeline(Tonemapping::kTonemapPipeline);

		commandList.SetRenderTarget(&frame.backBufferRTV.cpuAddress, 1, nullptr);

		device.CopyDescriptors(1, pData->histogramUAV.cpuAddress, histogramUAV.cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
		commandList.ClearUAVUInt(histogramUAV.gpuAddress, pData->histogramUAV.cpuAddress, pData->histogramBuffer.Get(), 0);

		float logLuminanceRange = pData->maxLogLuminance - pData->minLogLuminance;
		LuminanceHistogramParams histogramParams
		{
			.width = window.GetWidth(),
			.height = window.GetHeight(),
			.minLogLuminance = pData->minLogLuminance,
			.oneOverLogLuminanceRange = 1.0f / logLuminanceRange,
		};

		commandList.SetComputeRootSignature(*histogramPSO.pRootSignature);
		commandList.SetPipelineState(histogramPSO.pipelineState);
		commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
		Descriptor hdrTargetSRV = context.GetGPUDescriptor(frame, renderer.GetHDRTargetSRV());
		commandList.BindComputeGroup(1, hdrTargetSRV);
		commandList.PushComputeUAV(2, pData->histogramBuffer);
		commandList.Dispatch((histogramParams.width + 16 - 1) / 16, (histogramParams.height + 16 - 1) / 16, 1);

		barrier.PushTransition(pData->histogramBuffer, EResourceState::UAV, EResourceState::ShaderResource);
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

		barrier.PushTransition(pData->luminanceBuffer, EResourceState::UAV, EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Reset();

		Viewport viewport{ 0.0, 0.0, (float)window.GetWidth(), (float)window.GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, window.GetWidth(), window.GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetGraphicsRootSignature(*tonemapPSO.pRootSignature);
		commandList.SetPipelineState(tonemapPSO.pipelineState);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.BindGraphicsGroup(0, hdrTargetSRV);
		commandList.PushGraphicsSRV(1, pData->luminanceBuffer);
		commandList.DrawInstanced(3, 1, 0, 0);
	}

	void Tonemapping::Destroy(Renderer& renderer, TonemappingData* pData)
	{
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();
		srvHeap.Free(pData->histogramUAV);
		pData->histogramBuffer.Destroy();
		pData->luminanceBuffer.Destroy();
		delete pData;
	}
}
