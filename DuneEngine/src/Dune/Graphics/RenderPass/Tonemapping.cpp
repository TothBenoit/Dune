#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Scene/Camera.h"
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
		PSOCache& psoCache = *renderer.GetPSOCache();
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();

		TonemappingData* pData = new TonemappingData();

		const ShaderHandle fullScreenTriangleVS = psoCache.ResolveShader({ .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/FullScreenTriangle.hlsl"), .stage = EShaderStage::Vertex });
		const ShaderHandle tonemappingPS = psoCache.ResolveShader({ .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/Tonemapping.hlsl"), .stage = EShaderStage::Pixel });
		psoCache.ResolveRootSignature(tonemappingPS,
		{
			.layout =
			{
				{ .type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::Pixel },
				{ .type = EBindingType::SRV, .visibility = EShaderVisibility::Pixel },
			},
		});
		pData->tonemapPSO = psoCache.ResolvePSO(GraphicsPSODesc
		{
			.vertexShader = fullScreenTriangleVS,
			.pixelShader = tonemappingPS,
			.renderTargetCount = 1,
			.renderTargetsFormat = { EFormat::R8G8B8A8_UNORM },
		});

		const ShaderHandle histogramCS = psoCache.ResolveShader({ .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/LuminanceHistogram.hlsl"), .stage = EShaderStage::Compute });
		psoCache.ResolveRootSignature(histogramCS,
		{
			.layout =
			{
				{ .type = EBindingType::Constant, .byteSize = sizeof(LuminanceHistogramParams), .visibility = EShaderVisibility::All },
				{ .type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::All },
				{ .type = EBindingType::UAV, .visibility = EShaderVisibility::All },
			}
		});
		pData->histogramPSO = psoCache.ResolvePSO(ComputePSODesc{ .computeShader = histogramCS });

		pData->histogramBuffer.Initialize(device, { .debugName = L"HistogramBuffer", .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32)});
		pData->histogramUAV = srvHeap.Allocate();
		device.CreateUAV(pData->histogramUAV, pData->histogramBuffer, { .format = EFormat::R32_UINT, .elementCount = 256 });

		const ShaderHandle averageCS = psoCache.ResolveShader({ .path = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/LuminanceAverage.hlsl"), .stage = EShaderStage::Compute });
		psoCache.ResolveRootSignature(averageCS,
		{
			.layout =
			{
				{ .type = EBindingType::Constant, .byteSize = sizeof(LuminanceAverageParams), .visibility = EShaderVisibility::All },
				{ .type = EBindingType::SRV, .visibility = EShaderVisibility::All },
				{ .type = EBindingType::UAV, .visibility = EShaderVisibility::All },
			}
		});
		pData->averagePSO = psoCache.ResolvePSO(ComputePSODesc{ .computeShader = averageCS });

		pData->luminanceBuffer.Initialize(device, { .debugName = L"LuminanceBuffer", .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = sizeof(dU32)});

		return pData;
	}

	void Tonemapping::Execute(RenderPassContext& context, TonemappingData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		Descriptor histogramUAV = frame.srvHeap.Allocate(1);
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();
		Barrier& barrier = *context.pBarrier;
		PSOCache& psoCache = *renderer.GetPSOCache();

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

		commandList.SetComputeRootSignature(psoCache.GetRootSignature(psoCache.GetRootSignatureHandle(pData->histogramPSO)));
		commandList.SetPipelineState(psoCache.GetPipelineState(pData->histogramPSO));
		commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
		Descriptor hdrTargetSRV = frame.srvHeap.GetDescriptorAt(renderer.GetSRVHeap().GetIndex(frame.hdrTargetSRV) + context.pFrameData->sharedSRVHeapCapacity);
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
			.timeDelta = 0.016f,
			.tau = pData->tau
		};

		commandList.SetComputeRootSignature(psoCache.GetRootSignature(psoCache.GetRootSignatureHandle(pData->averagePSO)));
		commandList.SetPipelineState(psoCache.GetPipelineState(pData->averagePSO));
		commandList.PushComputeConstants(0, &averageParams, sizeof(averageParams));
		commandList.PushComputeSRV(1, pData->histogramBuffer);
		commandList.PushComputeUAV(2, pData->luminanceBuffer);
		commandList.Dispatch(1, 1, 1);

		barrier.PushTransition(pData->luminanceBuffer, EResourceState::UAV, EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Reset();

		commandList.SetGraphicsRootSignature(psoCache.GetRootSignature(psoCache.GetRootSignatureHandle(pData->tonemapPSO)));
		commandList.SetPipelineState(psoCache.GetPipelineState(pData->tonemapPSO));
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
