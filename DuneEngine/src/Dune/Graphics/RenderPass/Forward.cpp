#include "pch.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
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
#include "Dune/Component/Camera.h"

namespace Dune::Graphics
{
	void* Forward::Create(Renderer&)
	{
		return nullptr;
	}

	PipelineDesc Forward::GetPipelineDesc(MaterialKey key)
	{
		static const BindingSlot kLayout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = EShaderVisibility::All },
			{ .type = EBindingType::Constant, .byteSize = sizeof(InstanceData),   .visibility = EShaderVisibility::Vertex },
			{ .type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex),  .visibility = EShaderVisibility::Pixel },
		};

		const FileSystem::SerializationID<EFileType::Shader> shaderPath = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/Forward.hlsl");
		const EAlphaMode alphaMode = Material::GetAlphaMode(key);
		return PipelineDesc
		{
			.vertexShader = { .path = shaderPath, .stage = EShaderStage::Vertex },
			.pixelShader = { .path = shaderPath, .stage = EShaderStage::Pixel, .variantMask = alphaMode == EAlphaMode::Mask ? EShaderVariant::AlphaMask : EShaderVariant::None },
			.rootSignature = { .layout = kLayout, .allowInputLayout = true, .allowSRVHeapIndexing = true },
			.inputLayout =
			{
				VertexInput { .pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
				VertexInput { .pName = "NORMAL", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .isPerInstance = false },
				VertexInput { .pName = "TANGENT", .index = 0, .format = EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .isPerInstance = false },
				VertexInput { .pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false }
			},
			.cullingMode = Material::GetFaceCulling(key),
			.depthEnabled = true,
			.depthWrite = false,
			.depthFunc = alphaMode == EAlphaMode::Blend ? ECompFunc::LessEqual : ECompFunc::Equal,
			.renderTargetCount = 1,
			.renderTargetsFormat = { EFormat::R16G16B16A16_FLOAT },
			.renderTargetsBlendEnable = { alphaMode == EAlphaMode::Blend },
			.depthStencilFormat = EFormat::D32_FLOAT,
		};
	}

	void Forward::Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData)
	{
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pMaterialOutputs || !pVisibleSet)
			return;

		builder.Read(pMaterialOutputs->buffer, EResourceState::ShaderResource);

		Renderer& renderer = *context.pRenderer;
		builder.Write(renderer.GetHDRTargetHandle(), EResourceState::RenderTarget);
		builder.Write(renderer.GetDepthBufferHandle(), EResourceState::DepthStencil);

		const ShadowOutputs* pShadowOutputs = context.blackboard.TryGet<ShadowOutputs>();
		if (pShadowOutputs)
		{
			for (ResourceHandle handle : pShadowOutputs->shadows)
				builder.Read(handle, EResourceState::ShaderResource);
			builder.Read(pShadowOutputs->matrices, EResourceState::ShaderResource);
		}

		const LightOutputs* pLightOutputs = context.blackboard.TryGet<LightOutputs>();
		if (pLightOutputs)
			builder.Read(pLightOutputs->buffer, EResourceState::ShaderResource);
	}

	void Forward::Execute(RenderPassContext& context, void* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		const FrameData& frameData = *context.pFrameData;
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();

		Viewport viewport{ 0.0, 0.0, (float)window.GetWidth(), (float)window.GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, window.GetWidth(), window.GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(&renderer.GetHDRTargetRTV().cpuAddress, 1, &renderer.GetDepthBufferDSV().cpuAddress);

		ForwardGlobals globals;
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &globals.viewProjectionMatrix);
		globals.cameraPosition = context.pCamera->position;
		const LightOutputs* pLightOutputs = context.blackboard.TryGet<LightOutputs>();
		const ShadowOutputs* pShadowOutputs = context.blackboard.TryGet<ShadowOutputs>();
		const MaterialOutputs& materialOutputs = context.blackboard.Get<MaterialOutputs>();
		globals.lightCount = pLightOutputs ? pLightOutputs->count : 0;
		globals.lightBufferIndex = pLightOutputs ? pLightOutputs->bufferIndex : -1;
		globals.lightMatricesIndex = pShadowOutputs ? pShadowOutputs->matricesIndex : -1;
		globals.shadowStartIndex = pShadowOutputs ? pShadowOutputs->shadowStartIndex : -1;
		globals.materialBufferIndex = materialOutputs.bufferIndex;

		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

		const RootSignature* pBoundRootSignature = nullptr;
		MaterialKey boundKey = MaterialKey(-1);
		const VisibleSet& visibleSet = context.blackboard.Get<VisibleSet>();
		for (dSpan<const dU32> draws : { visibleSet.opaqueDraws, visibleSet.blendDraws })
		{
			for (dU32 drawIndex : draws)
			{
				const DrawItem& drawItem = frameData.drawItems[drawIndex];
				if (boundKey != drawItem.materialKey)
				{
					boundKey = drawItem.materialKey;
					PSO* pPSO = context.GetPSO(boundKey);
					Assert(pPSO);
					if (pBoundRootSignature != pPSO->pRootSignature)
					{
						pBoundRootSignature = pPSO->pRootSignature;
						commandList.SetGraphicsRootSignature(*pPSO->pRootSignature);
						commandList.PushGraphicsConstants(0, &globals, sizeof(ForwardGlobals));
					}
					commandList.SetPipelineState(pPSO->pipelineState);
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
		}
	}

	void Forward::Destroy(Renderer&, void*)
	{
	}
}
