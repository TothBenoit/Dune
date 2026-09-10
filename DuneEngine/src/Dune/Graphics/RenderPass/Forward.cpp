#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	ForwardData* Forward::Create(Renderer& renderer)
	{
		Device& device = *renderer.GetDevice();
		ForwardData* pData = new ForwardData();

		pData->forwardRS.Initialize(device,
		{
			.layout =
			{
				{.type = EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = EShaderVisibility::All},
				{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData),   .visibility = EShaderVisibility::Vertex},
				{.type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex),           .visibility = EShaderVisibility::Pixel},
			},
			.allowInputLayout = true,
			.allowSRVHeapIndexing = true,
		});

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };
		const wchar_t* maskedArgs[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug", L"-D", L"ALPHA_MASK"};

		Shader forwardVS;
		dWString shaderPath = StringUtils::ToWide(FileSystem::ResolvePath("engine://Shaders/Forward.hlsl"));
		ShaderDesc shaderDesc
		{
			.stage = EShaderStage::Vertex,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		};
		forwardVS.Initialize(shaderDesc);

		Shader forwardPS[2];
		shaderDesc.stage = EShaderStage::Pixel;
		shaderDesc.entryFunc = L"PSMain";
		forwardPS[0].Initialize(shaderDesc);
		shaderDesc.args = maskedArgs;
		shaderDesc.argsCount = _countof(maskedArgs);
		forwardPS[1].Initialize(shaderDesc);

		for (dU32 variant = 0; variant < Material::kVariantCount; variant++)
		{
			EAlphaMode alphaMode = Material::GetAlphaMode(variant);
			dU32 shaderIdx = alphaMode == EAlphaMode::Mask ? 1 : 0;
			pData->forwardPSO[variant].Initialize(device,
				{
					.pVertexShader = &forwardVS,
					.pPixelShader = &forwardPS[shaderIdx],
					.pRootSignature = &pData->forwardRS,
					.inputLayout =
					{
						VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
						VertexInput {.pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .isPerInstance = false },
						VertexInput {.pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .isPerInstance = false },
						VertexInput {.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false }
					},
					.rasterizerState = { .cullingMode = Material::IsDoubleSided(variant) ? ECullingMode::None : ECullingMode::Back },
					.depthStencilState = 
					{ 
						.depthFunc = alphaMode == EAlphaMode::Blend ? ECompFunc::LessEqual : ECompFunc::Equal, 
						.depthEnabled = true, 
						.depthWrite = false 
					},
					.renderTargetCount = 1,
					.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
					.renderTargetsBlend = { { .blendEnable = alphaMode == EAlphaMode::Blend } },
					.depthStencilFormat = EFormat::D32_FLOAT,
				}
			);
		}

		forwardVS.Destroy();
		for(Shader& ps : forwardPS)
			ps.Destroy();
		return pData;
	}

	void Forward::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ForwardData* pData)
	{
		Renderer& renderer = *context.pRenderer;

		builder.Write(renderer.GetHDRTargetHandle(), EResourceState::RenderTarget);
		builder.Write(renderer.GetDepthBufferHandle(), EResourceState::DepthStencil);
		builder.Read(renderer.Get<MaterialUpload>()->handle, EResourceState::ShaderResource);

		ShadowData* pShadowData = renderer.Get<Shadow>();
		for (ResourceHandle handle : pShadowData->activeHandles)
			builder.Read(handle, EResourceState::ShaderResource);
		if (!pShadowData->activeHandles.empty())
			builder.Read(pShadowData->matricesHandle, EResourceState::ShaderResource);

		LightUploadData* pLightData = renderer.Get<LightUpload>();
		if (pLightData->lightCount > 0)
			builder.Read(pLightData->handle, EResourceState::ShaderResource);
	}

	dU32 Draw(CommandList& commandList, const DrawItem& drawItem, dU32 currentVariant, const FrameData& frameData, ForwardData* pData)
	{
		dU32 variant = drawItem.materialVariant;
		if (currentVariant != variant)
		{
			commandList.SetPipelineState(pData->forwardPSO[variant]);
			currentVariant = variant;
		}

		commandList.PushGraphicsConstants(2, &drawItem.materialIdx, sizeof(MaterialIndex));

		InstanceData instanceData;
		instanceData.objectToWorld = drawItem.objectToWorld;
		commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));

		const GPUMeshView& mesh = frameData.meshes[drawItem.meshIdx];
		commandList.BindIndexBuffer(mesh.indicesGPUAddress, mesh.indicesByteSize, mesh.indicesAre32Bit);
		commandList.BindVertexBuffer(mesh.verticesGPUAddress, mesh.verticesByteSize, mesh.verticesByteStride);
		commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);

		return currentVariant;
	}

	void Forward::Execute(RenderPassContext& context, ForwardData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		const FrameData& frameData = *context.pFrameData;
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();

		Descriptor dsv = renderer.GetDepthBufferDSV();
		commandList.ClearRenderTargetView(frame.hdrTargetRTV, frame.hdrTarget.GetClearValue());

		Viewport viewport{ 0.0, 0.0, (float)window.GetWidth(), (float)window.GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, window.GetWidth(), window.GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(&frame.hdrTargetRTV.cpuAddress, 1, &dsv.cpuAddress);

		ForwardGlobals globals;
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &globals.viewProjectionMatrix);
		globals.cameraPosition = context.pCamera->position;
		const LightUploadData& lightUploadData = *renderer.Get<LightUpload>();
		const ShadowData& shadowData = *renderer.Get<Shadow>();
		const MaterialUploadData& materialUploadData = *renderer.Get<MaterialUpload>();
		globals.lightCount = lightUploadData.lightCount;
		globals.lightBufferIndex = lightUploadData.srvIndex + frameData.sharedSRVHeapCapacity;
		globals.lightMatricesIndex = shadowData.matricesPersistentSRVIndex + frameData.sharedSRVHeapCapacity;
		globals.shadowStartIndex = shadowData.shadowStartIndex;
		globals.materialBufferIndex = renderer.GetSRVHeap().GetIndex(materialUploadData.srv) + frameData.sharedSRVHeapCapacity;

		commandList.SetGraphicsRootSignature(pData->forwardRS);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));

		dU32 currentVariant = dU32(-1);
		for( dU32 drawIdx = 0; drawIdx < frameData.drawItems.size() - frameData.blendDrawCount; drawIdx++)
			currentVariant = Draw(commandList, frameData.drawItems[drawIdx], currentVariant, frameData, pData);
		for (dU32 drawIdx = 0; drawIdx < frameData.blendDrawCount; drawIdx++)
			currentVariant = Draw(commandList, frameData.drawItems[context.sortedBlendDraw[drawIdx]], currentVariant, frameData, pData);
	}

	void Forward::Destroy(Renderer&, ForwardData* pData)
	{
		for (PipelineState& pso : pData->forwardPSO)
			pso.Destroy();
		pData->forwardRS.Destroy();
		delete pData;
	}
}
