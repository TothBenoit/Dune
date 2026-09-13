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
#include "Dune/Graphics/Window.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	DepthPrepassData* DepthPrepass::Create(Renderer& renderer)
	{
		PSOCache& psoCache = *renderer.GetPSOCache();
		DepthPrepassData* pData = new DepthPrepassData();

		const FileSystem::SerializationID<EFileType::Shader> shaderPath = FileSystem::Resolve<EFileType::Shader>("engine://Shaders/DepthOnly.hlsl");
		const ShaderHandle depthVS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Vertex });
		const ShaderHandle depthMaskedVS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Vertex, .variantMask = EShaderVariant::AlphaMask });
		const ShaderHandle depthMaskedPS = psoCache.ResolveShader({ .path = shaderPath, .stage = EShaderStage::Pixel, .variantMask = EShaderVariant::AlphaMask });

		const BindingSlot layout[]
		{
			{ .type = EBindingType::Constant, .byteSize = sizeof(DepthGlobals),  .visibility = EShaderVisibility::All },
			{ .type = EBindingType::Constant, .byteSize = sizeof(InstanceData),  .visibility = EShaderVisibility::Vertex },
			{ .type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex), .visibility = EShaderVisibility::Pixel },
		};
		const RootSignatureDesc rootSignatureDesc{ .layout = layout, .allowInputLayout = true, .allowSRVHeapIndexing = true };
		psoCache.ResolveRootSignature(depthVS, rootSignatureDesc);
		psoCache.ResolveRootSignature(depthMaskedPS, rootSignatureDesc);

		const dVector<VertexInput> maskedVertexInputs
		{
			VertexInput{ .pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
			VertexInput{ .pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false },
		};

		const dVector<VertexInput> vertexInputs
		{
			VertexInput{ .pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
		};

		for (MaterialKey key = 0; key < Material::kKeyTableSize; key++)
		{
			EAlphaMode alphaMode = Material::GetAlphaMode(key);
			if (alphaMode == EAlphaMode::Blend)
			{
				pData->depthPSO[key] = kInvalidPSOHandle;
				continue;
			}

			const bool isMasked = alphaMode == EAlphaMode::Mask;
			pData->depthPSO[key] = psoCache.ResolvePSO(GraphicsPSODesc
			{
				.vertexShader = isMasked ? depthMaskedVS : depthVS,
				.pixelShader = isMasked ? depthMaskedPS : kInvalidShaderHandle,
				.inputLayout = isMasked ? maskedVertexInputs : vertexInputs,
				.cullingMode = Material::GetFaceCulling(key),
				.depthEnabled = true,
				.depthWrite = true,
				.depthStencilFormat = EFormat::D32_FLOAT,
			});
		}

		return pData;
	}

	void DepthPrepass::Setup(RenderGraphBuilder& builder, RenderPassContext& context, DepthPrepassData* pData)
	{
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		if (!pMaterialOutputs)
			return;
		builder.Read(pMaterialOutputs->buffer, EResourceState::ShaderResource);
		builder.Write(context.pRenderer->GetDepthBufferHandle(), EResourceState::DepthStencil);
	}

	void DepthPrepass::Execute(RenderPassContext& context, DepthPrepassData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();
		Window& window = *renderer.GetWindow();
		PSOCache& psoCache = *renderer.GetPSOCache();

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
		ComputeViewProjectionMatrix(*context.pCamera, nullptr, nullptr, &globals.viewProjectionMatrix);

		RootSignatureHandle boundRootSignature = kInvalidRootSignatureHandle;
		MaterialKey currentKey = MaterialKey(-1);
		for (dU32 drawIdx = 0; drawIdx < (dU32)frameData.drawItems.size() - frameData.blendDrawCount; drawIdx++)
		{
			const DrawItem& drawItem = frameData.drawItems[drawIdx];
			if (currentKey != drawItem.materialKey)
			{
				currentKey = drawItem.materialKey;
				const PSOHandle pso = pData->depthPSO[currentKey];
				Assert(pso != kInvalidPSOHandle);
				const RootSignatureHandle rootSignature = psoCache.GetRootSignatureHandle(pso);
				if (boundRootSignature != rootSignature)
				{
					boundRootSignature = rootSignature;
					commandList.SetGraphicsRootSignature(psoCache.GetRootSignature(rootSignature));
					commandList.PushGraphicsConstants(0, &globals, sizeof(DepthGlobals));
				}
				commandList.SetPipelineState(psoCache.GetPipelineState(pso));
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

	void DepthPrepass::Destroy(Renderer&, DepthPrepassData* pData)
	{
		delete pData;
	}
}
