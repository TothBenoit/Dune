#include "pch.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/Shader.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	void* Forward::Create(Renderer&)
	{
		return nullptr;
	}

	PipelineDesc Forward::GetPipelineDesc(MaterialKey key)
	{
		static const RHI::BindingSlot kLayout[]
		{
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(ForwardGlobals), .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(InstanceData),   .visibility = RHI::EShaderVisibility::Vertex },
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(MaterialIndex),  .visibility = RHI::EShaderVisibility::Pixel },
		};

		const PathID<RHI::Shader> shaderPath = FileSystem::Resolve<RHI::Shader>("engine://Shaders/Forward.hlsl");
		const EAlphaMode alphaMode = Material::GetAlphaMode(key);
		return PipelineDesc
		{
			.vertexShader = { .path = shaderPath, .stage = RHI::EShaderStage::Vertex },
			.pixelShader = { .path = shaderPath, .stage = RHI::EShaderStage::Pixel, .variantMask = alphaMode == EAlphaMode::Mask ? EShaderVariant::AlphaMask : EShaderVariant::None },
			.rootSignature = { .layout = kLayout, .allowInputLayout = true, .allowSRVHeapIndexing = true },
			.inputLayout =
			{
				RHI::VertexInput { .pName = "POSITION", .index = 0, .format = RHI::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
				RHI::VertexInput { .pName = "NORMAL", .index = 0, .format = RHI::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .isPerInstance = false },
				RHI::VertexInput { .pName = "TANGENT", .index = 0, .format = RHI::EFormat::R32G32B32A32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .isPerInstance = false },
				RHI::VertexInput { .pName = "UV", .index = 0, .format = RHI::EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false }
			},
			.cullingMode = Material::GetFaceCulling(key),
			.depthEnabled = true,
			.depthWrite = false,
			.depthFunc = alphaMode == EAlphaMode::Blend ? RHI::ECompFunc::LessEqual : RHI::ECompFunc::Equal,
			.renderTargetCount = 1,
			.renderTargetsFormat = { RHI::EFormat::R16G16B16A16_FLOAT },
			.renderTargetsBlendEnable = { alphaMode == EAlphaMode::Blend },
			.depthStencilFormat = RHI::EFormat::D32_FLOAT,
		};
	}

	void Forward::Setup(RenderPassContext& context, void* pData)
	{
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pMaterialOutputs || !pVisibleSet)
			return;

		RenderGraphBuilder& builder = context.AddPass();
		const RGBufferSRV materialsSRV = builder.ReadSRV(pMaterialOutputs->buffer);

		Renderer& renderer = *context.pRenderer;
		const RGTextureRTV hdrRTV = builder.WriteRTV(context.blackboard.Get<SceneResourceOutputs>().hdrTarget);
		const RGTextureDSV depthDSV = builder.ReadDSV(context.blackboard.Get<SceneResourceOutputs>().depthBuffer);

		RGBufferSRV shadowsSRV;
		const ShadowOutputs* pShadowOutputs = context.blackboard.TryGet<ShadowOutputs>();
		if (pShadowOutputs)
		{
			for (RGTexture handle : pShadowOutputs->shadowMaps)
				builder.Read(handle, RHI::EResourceState::ShaderResource);
			shadowsSRV = builder.ReadSRV(pShadowOutputs->shadows);
		}

		RGBufferSRV lightsSRV;
		const LightOutputs* pLightOutputs = context.blackboard.TryGet<LightOutputs>();
		if (pLightOutputs)
			lightsSRV = builder.ReadSRV(pLightOutputs->buffer);

		builder.OnExecute([hdrRTV, depthDSV, materialsSRV, shadowsSRV, lightsSRV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				const FrameData& frameData = *context.pFrameData;
				RHI::CommandList& commandList = frame.commandList;
				RHI::Device& device = *renderer.GetDevice();
				const dUVec2 resolution = renderer.GetResolution();

				RHI::Viewport viewport{ 0.0, 0.0, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f };
				RHI::Scissor scissor{ 0, 0, resolution.x, resolution.y };
				commandList.SetViewports(1, &viewport);
				commandList.SetScissors(1, &scissor);
				const RHI::Descriptor rtv = context.GetDescriptor(hdrRTV);
				const RHI::Descriptor dsv = context.GetDescriptor(depthDSV);
				commandList.SetRenderTarget(&rtv.cpuAddress, 1, &dsv.cpuAddress);

				ForwardGlobals globals;
				globals.viewProjectionMatrix = context.pView->viewProjection;
				globals.cameraPosition = context.pView->position;
				const LightOutputs* pLightOutputs = context.blackboard.TryGet<LightOutputs>();
				globals.lightCount = pLightOutputs ? pLightOutputs->count : 0;
				globals.lightBufferIndex = lightsSRV.IsValid() ? context.GetBindlessIndex(context.GetDescriptor(lightsSRV)) : -1;
				globals.shadowsIndex = shadowsSRV.IsValid() ? context.GetBindlessIndex(context.GetDescriptor(shadowsSRV)) : -1;
				globals.materialBufferIndex = context.GetBindlessIndex(context.GetDescriptor(materialsSRV));

				commandList.SetPrimitiveTopology(RHI::EPrimitiveTopology::TriangleList);

				const RHI::RootSignature* pBoundRootSignature = nullptr;
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

						const GPUMeshView& mesh = frameData.pResources->meshes[drawItem.meshIdx];
						commandList.BindIndexBuffer(mesh.indicesGPUAddress, mesh.indicesByteSize, mesh.indicesAre32Bit);
						commandList.BindVertexBuffer(mesh.verticesGPUAddress, mesh.verticesByteSize, mesh.verticesByteStride);
						commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
					}
				}
			});
	}

	void Forward::Destroy(Renderer&, void*)
	{
	}
}
