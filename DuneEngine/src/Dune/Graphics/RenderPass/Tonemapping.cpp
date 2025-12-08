#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	void Tonemapping::Initialize(Renderer& renderer)
	{
		Device* pDevice = renderer.GetDevice();

		m_barrier.Initialize(1);

		m_heap.Initialize(pDevice, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = 1, .isShaderVisible = false });

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

		m_tonemapRS.Initialize(pDevice,
			{
				.layout =
				{
					{.type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::Pixel},
					{.type = EBindingType::SRV, .visibility = EShaderVisibility::Pixel},
				},
			});

		m_tonemapPSO.Initialize(pDevice,
			{
				.pVertexShader = &fullScreenTriangleVS,
				.pPixelShader = &tonemappingPS,
				.pRootSignature = &m_tonemapRS,
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

		m_histogramRS.Initialize(pDevice,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(LuminanceHistogramParams), .visibility = EShaderVisibility::All},
					{.type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::All},
					{.type = EBindingType::UAV, .visibility = EShaderVisibility::All},
				}
			});

		m_histogramPSO.Initialize(pDevice,
			{
				.pComputeShader = &histogramCS,
				.pRootSignature = &m_histogramRS
			});

		m_histogramBuffer.Initialize(pDevice, { .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = 256 * sizeof(dU32) });
		m_histogramUAV = m_heap.Allocate();
		pDevice->CreateUAV(m_histogramUAV, m_histogramBuffer, { .format = EFormat::R32_UINT, .elementCount = 256 });

		Shader averageCS;
		averageCS.Initialize
		({
			.stage = EShaderStage::Compute,
			.filePath = L"Shaders\\LuminanceAverage.hlsl",
			.entryFunc = L"CSMain",
			.args = args,
			.argsCount = _countof(args),
		});

		m_averageRS.Initialize(pDevice,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(LuminanceAverageParams), .visibility = EShaderVisibility::All},
					{.type = EBindingType::SRV, .visibility = EShaderVisibility::All},
					{.type = EBindingType::UAV, .visibility = EShaderVisibility::All},
				}
			});

		m_averagePSO.Initialize(pDevice,
			{
				.pComputeShader = &averageCS,
				.pRootSignature = &m_averageRS
			});

		m_luminanceBuffer.Initialize(pDevice, { .usage = EBufferUsage::UAV, .memory = EBufferMemory::GPU, .byteSize = sizeof(dU32) });

		fullScreenTriangleVS.Destroy();
		tonemappingPS.Destroy();
		histogramCS.Destroy();
		averageCS.Destroy();
	}

	void Tonemapping::Destroy()
	{
		m_heap.Free(m_histogramUAV);
		m_heap.Destroy();

		m_histogramRS.Destroy();
		m_histogramPSO.Destroy();
		m_histogramBuffer.Destroy();

		m_averageRS.Destroy();
		m_averagePSO.Destroy();
		m_luminanceBuffer.Destroy();

		m_tonemapPSO.Destroy();
		m_tonemapRS.Destroy();

		m_barrier.Destroy();
	}

	void Tonemapping::Render(Renderer& renderer, CommandList& commandList, Descriptor& hdrTargetSRV)
	{
		Frame& frame = renderer.GetCurrentFrame();
		Descriptor histogramUAV = frame.srvHeap.Allocate(1);
		Device* pDevice = renderer.GetDevice();
		Window* pWindow = renderer.GetWindow();

		pDevice->CopyDescriptors(1, m_histogramUAV.cpuAddress, histogramUAV.cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
		commandList.ClearUAVUInt(histogramUAV.gpuAddress, m_histogramUAV.cpuAddress, m_histogramBuffer.Get(), 0);

		float logLuminanceRange = m_maxLogLuminance - m_minLogLuminance;
		LuminanceHistogramParams histogramParams
		{
			.width = pWindow->GetWidth(),
			.height = pWindow->GetWidth(),
			.minLogLuminance = m_minLogLuminance,
			.oneOverLogLuminanceRange = 1.0f / logLuminanceRange,
		};
		
		commandList.SetComputeRootSignature(m_histogramRS);
		commandList.SetPipelineState(m_histogramPSO);
		commandList.PushComputeConstants(0, &histogramParams, sizeof(histogramParams));
		commandList.BindComputeGroup(1, hdrTargetSRV);
		commandList.PushComputeUAV(2, m_histogramBuffer);
		commandList.Dispatch((histogramParams.width + 16 - 1) / 16, (histogramParams.height + 16 - 1) / 16, 1);

		m_barrier.PushTransition(m_histogramBuffer.Get(), EResourceState::UAV, EResourceState::ShaderResource);
		commandList.Transition(m_barrier);
		m_barrier.Reset();

		LuminanceAverageParams averageParams
		{
			.pixelCount = histogramParams.width * histogramParams.height,
			.minLogLuminance = m_minLogLuminance,
			.logLuminanceRange = logLuminanceRange,
			.timeDelta = 0.016f,
			.tau = m_tau
		};

		commandList.SetComputeRootSignature(m_averageRS);
		commandList.SetPipelineState(m_averagePSO);
		commandList.PushComputeConstants(0, &averageParams, sizeof(averageParams));
		commandList.PushComputeSRV(1, m_histogramBuffer);
		commandList.PushComputeUAV(2, m_luminanceBuffer);
		commandList.Dispatch(1, 1, 1);

		m_barrier.PushTransition(m_luminanceBuffer.Get(), EResourceState::UAV, EResourceState::ShaderResource);
		commandList.Transition(m_barrier);
		m_barrier.Reset();

		commandList.SetGraphicsRootSignature(m_tonemapRS);
		commandList.SetPipelineState(m_tonemapPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.BindGraphicsGroup(0, hdrTargetSRV);
		commandList.PushGraphicsSRV(1, m_luminanceBuffer);
		commandList.DrawInstanced(3, 1, 0, 0);
	}
}