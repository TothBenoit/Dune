#include "pch.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
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
	void* DepthPrepass::Create(Renderer&)
	{
		return nullptr;
	}

	bool DepthPrepass::IsPassMaterial(MaterialKey key)
	{
		return Material::GetAlphaMode(key) != EAlphaMode::Blend;
	}

	PipelineDesc DepthPrepass::GetPipelineDesc(MaterialKey key)
	{
		const EAlphaMode alphaMode = Material::GetAlphaMode(key);
		Assert(alphaMode != EAlphaMode::Blend);

		static const RHI::BindingSlot kLayout[]
		{
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(DepthGlobals),  .visibility = RHI::EShaderVisibility::All },
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(InstanceData),  .visibility = RHI::EShaderVisibility::Vertex },
			{ .type = RHI::EBindingType::Constant, .byteSize = sizeof(MaterialIndex), .visibility = RHI::EShaderVisibility::Pixel },
		};
		const RHI::VertexInput position{ .pName = "POSITION", .index = 0, .format = RHI::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false };
		const RHI::VertexInput uv{ .pName = "UV", .index = 0, .format = RHI::EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false };

		const PathID<RHI::Shader> shaderPath = FileSystem::Resolve<RHI::Shader>("engine://Shaders/DepthOnly.hlsl");
		const bool isMasked = alphaMode == EAlphaMode::Mask;
		return PipelineDesc
		{
			.vertexShader = { .path = shaderPath, .stage = RHI::EShaderStage::Vertex, .variantMask = isMasked ? EShaderVariant::AlphaMask : EShaderVariant::None },
			.pixelShader = isMasked ? ShaderEntryDesc{ .path = shaderPath, .stage = RHI::EShaderStage::Pixel, .variantMask = EShaderVariant::AlphaMask } : ShaderEntryDesc{},
			.rootSignature = { .layout = kLayout, .allowInputLayout = true, .allowSRVHeapIndexing = true },
			.inputLayout = isMasked ? dVector<RHI::VertexInput>{ position, uv } : dVector<RHI::VertexInput>{ position },
			.cullingMode = Material::GetFaceCulling(key),
			.depthEnabled = true,
			.depthWrite = true,
			.depthStencilFormat = RHI::EFormat::D32_FLOAT,
		};
	}

	void DepthPrepass::Setup(RenderPassContext& context, void* pData)
	{
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pMaterialOutputs || !pVisibleSet)
			return;

		RenderGraphBuilder& builder = context.AddPass();
		const RGBufferSRV materialsSRV = builder.ReadSRV(pMaterialOutputs->buffer);
		const RGTextureDSV depthDSV = builder.WriteDSV(context.blackboard.Get<SceneResourceOutputs>().depthBuffer);

		builder.OnExecute([depthDSV, materialsSRV](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				RHI::CommandList& commandList = frame.commandList;
				RHI::Device& device = *renderer.GetDevice();
				const dUVec2 resolution = renderer.GetResolution();

				RHI::Descriptor dsv = context.GetDescriptor(depthDSV);
				RHI::Viewport viewport{ 0.0, 0.0, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f };
				RHI::Scissor scissor{ 0, 0, resolution.x, resolution.y };
				commandList.SetViewports(1, &viewport);
				commandList.SetScissors(1, &scissor);
				commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

				commandList.SetPrimitiveTopology(RHI::EPrimitiveTopology::TriangleList);

				const FrameData& frameData = *context.pFrameData;
				DepthGlobals globals
				{
					.materialBufferIndex = context.GetBindlessIndex(context.GetDescriptor(materialsSRV))
				};
				globals.viewProjectionMatrix = context.pView->viewProjection;

				const RHI::RootSignature* pBoundRootSignature = nullptr;
				MaterialKey boundKey = MaterialKey(-1);
				for (dU32 drawIndex : context.blackboard.Get<VisibleSet>().opaqueDraws)
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
							commandList.PushGraphicsConstants(0, &globals, sizeof(DepthGlobals));
						}
						commandList.SetPipelineState(pPSO->pipelineState);
					}

					if (Material::GetAlphaMode(drawItem.materialKey) == EAlphaMode::Mask)
						commandList.PushGraphicsConstants(2, &drawItem.materialIdx, sizeof(MaterialIndex));

					InstanceData instanceData;
					instanceData.objectToWorld = drawItem.objectToWorld;
					commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));
					const GPUMeshView& mesh = frameData.pResources->meshes[drawItem.meshIdx];
					commandList.BindIndexBuffer(mesh.indicesGPUAddress, mesh.indicesByteSize, mesh.indicesAre32Bit);
					commandList.BindVertexBuffer(mesh.verticesGPUAddress, mesh.verticesByteSize, mesh.verticesByteStride);
					commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
				}
			});
	}

	void DepthPrepass::Destroy(Renderer&, void*)
	{
	}
}
