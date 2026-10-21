#include "pch.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Platform/Window.h"
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

		static const BindingSlot kLayout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(DepthGlobals),  .visibility = EShaderVisibility::All },
			{ .type = EBindingType::Constant, .byteSize = sizeof(InstanceData),  .visibility = EShaderVisibility::Vertex },
			{ .type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex), .visibility = EShaderVisibility::Pixel },
		};
		const VertexInput position{ .pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false };
		const VertexInput uv{ .pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false };

		const FileSystem::SerializationID<EFileType::Shader> shaderPath = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/DepthOnly.hlsl");
		const bool isMasked = alphaMode == EAlphaMode::Mask;
		return PipelineDesc
		{
			.vertexShader = { .path = shaderPath, .stage = EShaderStage::Vertex, .variantMask = isMasked ? EShaderVariant::AlphaMask : EShaderVariant::None },
			.pixelShader = isMasked ? ShaderEntryDesc{ .path = shaderPath, .stage = EShaderStage::Pixel, .variantMask = EShaderVariant::AlphaMask } : ShaderEntryDesc{},
			.rootSignature = { .layout = kLayout, .allowInputLayout = true, .allowSRVHeapIndexing = true },
			.inputLayout = isMasked ? dVector<VertexInput>{ position, uv } : dVector<VertexInput>{ position },
			.cullingMode = Material::GetFaceCulling(key),
			.depthEnabled = true,
			.depthWrite = true,
			.depthStencilFormat = EFormat::D32_FLOAT,
		};
	}

	void DepthPrepass::Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData)
	{
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pMaterialOutputs || !pVisibleSet)
			return;

		builder.Read(pMaterialOutputs->buffer, EResourceState::ShaderResource);
		builder.Write(context.pRenderer->GetDepthBufferHandle(), EResourceState::DepthStencil);
	}

	void DepthPrepass::Execute(RenderPassContext& context, void* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();

		Descriptor dsv = renderer.GetDepthBufferDSV();
		Viewport viewport{ 0.0, 0.0, (float)window.GetWidth(), (float)window.GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, window.GetWidth(), window.GetHeight() };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);
		commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

		const FrameData& frameData = *context.pFrameData;
		DepthGlobals globals
		{
			.materialBufferIndex = context.blackboard.Get<MaterialOutputs>().bufferIndex
		};
		globals.viewProjectionMatrix = context.pView->viewProjection;

		const RootSignature* pBoundRootSignature = nullptr;
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
			const GPUMeshView& mesh = frameData.meshes[drawItem.meshIdx];
			commandList.BindIndexBuffer(mesh.indicesGPUAddress, mesh.indicesByteSize, mesh.indicesAre32Bit);
			commandList.BindVertexBuffer(mesh.verticesGPUAddress, mesh.verticesByteSize, mesh.verticesByteStride);
			commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
		}
	}

	void DepthPrepass::Destroy(Renderer&, void*)
	{
	}
}
