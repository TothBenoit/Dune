#include "pch.h"
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
		PSOCache& psoCache = *renderer.GetPSOCache();
		ForwardData* pData = new ForwardData();

		const FileSystem::SerializationID<EFileType::Shader> shaderPath = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/Forward.hlsl");
		const ShaderHandle forwardVS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Vertex });
		const ShaderHandle forwardPS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Pixel });
		const ShaderHandle forwardMaskedPS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Pixel, .variantMask = EShaderVariant::AlphaMask });

		const BindingSlot layout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = EShaderVisibility::All },
			{ .type = EBindingType::Constant, .byteSize = sizeof(InstanceData),   .visibility = EShaderVisibility::Vertex },
			{ .type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex),  .visibility = EShaderVisibility::Pixel },
		};
		const RootSignatureDesc rootSignatureDesc{ .layout = layout, .allowInputLayout = true, .allowSRVHeapIndexing = true };
		psoCache.ResolveRootSignature(forwardPS, rootSignatureDesc);
		psoCache.ResolveRootSignature(forwardMaskedPS, rootSignatureDesc);

		const dVector<VertexInput> vertexInputs
		{
			VertexInput { .pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
			VertexInput { .pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .isPerInstance = false },
			VertexInput { .pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .isPerInstance = false },
			VertexInput { .pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false }
		};

		for (dU32 variant = 0; variant < Material::kVariantCount; variant++)
		{
			const EAlphaMode alphaMode = Material::GetAlphaMode(variant);
			pData->forwardPSO[variant] = psoCache.ResolvePSO(GraphicsPSODesc
			{
				.vertexShader = forwardVS,
				.pixelShader = alphaMode == EAlphaMode::Mask ? forwardMaskedPS : forwardPS,
				.inputLayout = vertexInputs,
				.cullingMode = Material::IsDoubleSided(variant) ? ECullingMode::None : ECullingMode::Back,
				.depthEnabled = true,
				.depthWrite = false,
				.depthFunc = alphaMode == EAlphaMode::Blend ? ECompFunc::LessEqual : ECompFunc::Equal,
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
				.renderTargetsBlendEnable = { alphaMode == EAlphaMode::Blend },
				.depthStencilFormat = EFormat::D32_FLOAT,
			});
		}

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

	static void Draw(CommandList& commandList, PSOCache& psoCache, const ForwardGlobals& globals, const DrawItem& drawItem, const FrameData& frameData, ForwardData* pData, dU32& currentVariant, RootSignatureHandle& boundRootSignature)
	{
		const dU32 variant = drawItem.materialVariant;
		if (currentVariant != variant)
		{
			currentVariant = variant;
			const PSOHandle pso = pData->forwardPSO[variant];
			const RootSignatureHandle rootSignature = psoCache.GetRootSignatureHandle(pso);
			if (boundRootSignature != rootSignature)
			{
				boundRootSignature = rootSignature;
				commandList.SetGraphicsRootSignature(psoCache.GetRootSignature(rootSignature));
				commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));
			}
			commandList.SetPipelineState(psoCache.GetPipelineState(pso));
		}

		commandList.PushGraphicsConstants(2, &drawItem.materialIdx, sizeof(MaterialIndex));

		InstanceData instanceData;
		instanceData.objectToWorld = drawItem.objectToWorld;
		commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));

		const GPUMeshView& mesh = frameData.meshes[drawItem.meshIdx];
		commandList.BindIndexBuffer(mesh.indicesGPUAddress, mesh.indicesByteSize, mesh.indicesAre32Bit);
		commandList.BindVertexBuffer(mesh.verticesGPUAddress, mesh.verticesByteSize, mesh.verticesByteStride);
		commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
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
		PSOCache& psoCache = *renderer.GetPSOCache();

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

		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

		dU32 currentVariant = dU32(-1);
		RootSignatureHandle boundRootSignature = kInvalidRootSignatureHandle;
		for( dU32 drawIdx = 0; drawIdx < frameData.drawItems.size() - frameData.blendDrawCount; drawIdx++)
			Draw(commandList, psoCache, globals, frameData.drawItems[drawIdx], frameData, pData, currentVariant, boundRootSignature);
		for (dU32 drawIdx = 0; drawIdx < frameData.blendDrawCount; drawIdx++)
			Draw(commandList, psoCache, globals, frameData.drawItems[context.sortedBlendDraw[drawIdx]], frameData, pData, currentVariant, boundRootSignature);
	}

	void Forward::Destroy(Renderer&, ForwardData* pData)
	{
		delete pData;
	}
}
