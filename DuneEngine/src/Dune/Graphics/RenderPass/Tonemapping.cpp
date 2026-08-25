#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	RenderPassDesc Tonemapping::GetDesc()
	{
		RenderPassDesc desc
		{
#ifdef _DEBUG
			.name = "Tonemapping",
#endif
			.reads =
			{
				{ .id = EResourceTag::HDRTarget, .state = EResourceState::ShaderResource },
			},
			.writes = 
			{
				{ .id = EResourceTag::OutputTarget, .state = EResourceState::RenderTarget },
			}
		};
		return desc;
	}

	TonemappingData* Tonemapping::Create(Renderer& renderer)
	{
		Device& device = renderer.GetRenderContext()->GetDevice();
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();

		TonemappingData* pData = new TonemappingData();

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };

		Shader fullScreenTriangleVS;
		fullScreenTriangleVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\FullScreenTriangle.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
			});


		Shader tonemappingPS;
		tonemappingPS.Initialize
		({
			.stage = EShaderStage::Pixel,
			.filePath = L"Shaders\\Tonemapping.hlsl",
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		pData->tonemapRS.Initialize(device,
		{
			.layout =
			{
				{.type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::Pixel},
				{.type = EBindingType::SRV, .visibility = EShaderVisibility::Pixel},
			},
		});

		pData->tonemapPSO.Initialize(device,
			{
				.pVertexShader = &fullScreenTriangleVS,
				.pPixelShader = &tonemappingPS,
				.pRootSignature = &pData->tonemapRS,
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R8G8B8A8_UNORM },
			});

		Shader histogramCS;
		histogramCS.Initialize
		({
			.stage = EShaderStage::Compute,
			.filePath = L"Shaders\\LuminanceHistogram.hlsl",
			.entryFunc = L"CSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		pData->histogramRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(LuminanceHistogramParams), .visibility = EShaderVisibility::All},
					{.type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::All},
					{.type = EBindingType::UAV, .visibility = EShaderVisibility::All},
				}
			});

		pData->histogramPSO.Initialize(device,
			{
				.pComputeShader = &histogramCS,
				.pRootSignature = &pData->histogramRS
			});

		pData->histogramBuffer.Initialize(device, { .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32) });
		pData->histogramUAV = srvHeap.Allocate();
		device.CreateUAV(pData->histogramUAV, pData->histogramBuffer, { .format = EFormat::R32_UINT, .elementCount = 256 });

		Shader averageCS;
		averageCS.Initialize
		({
			.stage = EShaderStage::Compute,
			.filePath = L"Shaders\\LuminanceAverage.hlsl",
			.entryFunc = L"CSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		pData->averageRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(LuminanceAverageParams), .visibility = EShaderVisibility::All},
					{.type = EBindingType::SRV, .visibility = EShaderVisibility::All},
					{.type = EBindingType::UAV, .visibility = EShaderVisibility::All},
				}
			});

		pData->averagePSO.Initialize(device,
			{
				.pComputeShader = &averageCS,
				.pRootSignature = &pData->averageRS
			});

		pData->luminanceBuffer.Initialize(device, { .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = sizeof(dU32) });

		fullScreenTriangleVS.Destroy();
		tonemappingPS.Destroy();
		histogramCS.Destroy();
		averageCS.Destroy();

		return pData;
	}

	void Tonemapping::Execute(RenderPassContext& context, TonemappingData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		Descriptor histogramUAV = frame.srvHeap.Allocate(1);
		Device& device = renderer.GetRenderContext()->GetDevice();
		Window* pWindow = renderer.GetWindow();
		Barrier& barrier = *context.pBarrier;

		device.CopyDescriptors(1, pData->histogramUAV.cpuAddress, histogramUAV.cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
		commandList.ClearUAVUInt(histogramUAV.gpuAddress, pData->histogramUAV.cpuAddress, pData->histogramBuffer.Get(), 0);

		float logLuminanceRange = pData->maxLogLuminance - pData->minLogLuminance;
		LuminanceHistogramParams histogramParams
		{
			.width = pWindow->GetWidth(),
			.height = pWindow->GetHeight(),
			.minLogLuminance = pData->minLogLuminance,
			.oneOverLogLuminanceRange = 1.0f / logLuminanceRange,
		};

		commandList.SetComputeRootSignature(pData->histogramRS);
		commandList.SetPipelineState(pData->histogramPSO);
		commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
		Descriptor hdrTargetSRV = frame.srvHeap.GetDescriptorAt(renderer.GetSRVHeap().GetIndex(frame.hdrTargetSRV));
		commandList.BindComputeGroup(1, hdrTargetSRV);
		commandList.PushComputeUAV(2, pData->histogramBuffer);
		commandList.Dispatch((histogramParams.width + 16 - 1) / 16, (histogramParams.height + 16 - 1) / 16, 1);

		barrier.PushTransition(pData->histogramBuffer.Get(), EResourceState::UAV, EResourceState::ShaderResource);
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

		commandList.SetComputeRootSignature(pData->averageRS);
		commandList.SetPipelineState(pData->averagePSO);
		commandList.PushComputeConstants(0, &averageParams, sizeof(averageParams));
		commandList.PushComputeSRV(1, pData->histogramBuffer);
		commandList.PushComputeUAV(2, pData->luminanceBuffer);
		commandList.Dispatch(1, 1, 1);

		barrier.PushTransition(pData->luminanceBuffer.Get(), EResourceState::UAV, EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Reset();

		commandList.SetGraphicsRootSignature(pData->tonemapRS);
		commandList.SetPipelineState(pData->tonemapPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.BindGraphicsGroup(0, hdrTargetSRV);
		commandList.PushGraphicsSRV(1, pData->luminanceBuffer);
		commandList.DrawInstanced(3, 1, 0, 0);
	}

	void Tonemapping::Destroy(Renderer& renderer, TonemappingData* pData)
	{
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();
		srvHeap.Free(pData->histogramUAV);
		pData->histogramRS.Destroy();
		pData->histogramPSO.Destroy();
		pData->histogramBuffer.Destroy();

		pData->averageRS.Destroy();
		pData->averagePSO.Destroy();
		pData->luminanceBuffer.Destroy();

		pData->tonemapPSO.Destroy();
		pData->tonemapRS.Destroy();
		delete pData;
	}
}
