#include "pch.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
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
	static const RHI::TextureDesc kShadowMapDesc
	{
		.debugName = L"ShadowMap",
		.usage = RHI::ETextureUsage::DepthStencil | RHI::ETextureUsage::ShaderResource,
		.width = SHADOW_MAP_RESOLUTION,
		.height = SHADOW_MAP_RESOLUTION,
		.mipLevels = 1,
		.format = RHI::EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = RHI::EResourceState::DepthStencil
	};

	static const RHI::TextureDesc kCubeShadowMapDesc
	{
		.debugName = L"CubeShadowMap",
		.usage = RHI::ETextureUsage::DepthStencil | RHI::ETextureUsage::ShaderResource,
		.dimension = RHI::ETextureDimension::TextureCube,
		.width = SHADOW_MAP_RESOLUTION,
		.height = SHADOW_MAP_RESOLUTION,
		.layer = 6,
		.mipLevels = 1,
		.format = RHI::EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = RHI::EResourceState::DepthStencil
	};

	ShadowPassData* Shadow::Create(Renderer& renderer)
	{
		return new ShadowPassData();
	}

	bool Shadow::IsPassMaterial(MaterialKey key)
	{
		return Material::GetAlphaMode(key) != EAlphaMode::Blend;
	}

	PipelineDesc Shadow::GetPipelineDesc(MaterialKey key)
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
			.depthBias = 10,
			.slopeScaledDepthBias = 4.0f,
			.depthClipEnable = false,
			.depthEnabled = true,
			.depthWrite = true,
			.depthStencilFormat = RHI::EFormat::D32_FLOAT,
		};
	}

	static void RenderDepth(RenderPassContext& context, RGBufferSRV materialsSRV, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		RHI::CommandList& commandList = frame.commandList;
		RHI::Device& device = *renderer.GetDevice();

		commandList.SetPrimitiveTopology(RHI::EPrimitiveTopology::TriangleList);

		const FrameData& frameData = *context.pFrameData;
		DepthGlobals globals
		{
			.viewProjectionMatrix = viewProjection,
			.materialBufferIndex = context.GetBindlessIndex(context.GetDescriptor(materialsSRV)),
		};

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
	}

	static dMatrix4x4 GetFaceMatrix(const LightData& light, const dMatrix4x4& lightMatrix, dU32 faceIndex)
	{
		if (!light.IsPoint())
			return lightMatrix;

		dVec eye{ light.position.x, light.position.y, light.position.z };
		dMatrix viewMatrices[]
		{
			DirectX::XMMatrixLookToLH(eye, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f }),
			DirectX::XMMatrixLookToLH(eye, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f }),
			DirectX::XMMatrixLookToLH(eye, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f, -1.0f }),
			DirectX::XMMatrixLookToLH(eye, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f,  1.0f }),
			DirectX::XMMatrixLookToLH(eye, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f,  0.0f }),
			DirectX::XMMatrixLookToLH(eye, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f,  0.0f }),
		};

		dMatrix4x4 faceMatrix;
		DirectX::XMStoreFloat4x4(&faceMatrix, viewMatrices[faceIndex] * DirectX::XMLoadFloat4x4(&lightMatrix));
		return faceMatrix;
	}

	void Shadow::Setup(RenderPassContext& context, ShadowPassData* pData)
	{
		pData->activeHandles.clear();
		pData->shadows.clear();
		const MaterialOutputs* pMaterialOutputs = context.blackboard.TryGet<MaterialOutputs>();
		if (!pMaterialOutputs)
			return;

		const VisibleSet* pVisibleSet = context.blackboard.TryGet<VisibleSet>();
		if (!pVisibleSet)
			return;
		const dSpan<const dU32> shadowCasters = pVisibleSet->shadowCasters;
		if (shadowCasters.IsEmpty())
			return;

		RenderGraph& graph = context.GetRenderGraph();
		dU32 shadowCount = shadowCasters.GetSize();

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < shadowCount; casterIndex++)
		{
			const LightData& light = pVisibleSet->lights[shadowCasters[casterIndex]];
			RGTexture handle;
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
					pData->cubeShadowHandles.push_back(graph.CreateTexture(kCubeShadowMapDesc));
				handle = pData->cubeShadowHandles[cubeShadowIndex++];
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
					pData->shadowHandles.push_back(graph.CreateTexture(kShadowMapDesc));
				handle = pData->shadowHandles[shadowIndex++];
			}
			pData->activeHandles.push_back(handle);
			pData->shadows.push_back({ .shadowMatrix = pVisibleSet->shadowMatrices[casterIndex], .shadowIndex = context.GetBindlessIndex(graph.GetDefaultSRV(handle)) });

			const dU32 faceCount = light.IsPoint() ? 6 : 1;
			for (dU32 faceIndex = 0; faceIndex < faceCount; faceIndex++)
			{
				RenderGraphBuilder& builder = context.AddPass();
				const RGBufferSRV materialsSRV = builder.ReadSRV(pMaterialOutputs->buffer);
				const RGTextureDSV depthDSV = builder.WriteDSV(handle, 0, faceIndex);

				builder.OnExecute([depthDSV, materialsSRV, casterIndex, faceIndex](RenderPassContext& context)
					{
						RHI::CommandList& commandList = context.pRenderer->GetCurrentFrame().commandList;
						const VisibleSet& visibleSet = context.blackboard.Get<VisibleSet>();
						const LightData& light = visibleSet.lights[visibleSet.shadowCasters[casterIndex]];

						RHI::Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
						RHI::Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
						commandList.SetViewports(1, &viewport);
						commandList.SetScissors(1, &scissor);

						const RHI::Descriptor dsv = context.GetDescriptor(depthDSV);
						commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
						commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
						RenderDepth(context, materialsSRV, GetFaceMatrix(light, visibleSet.shadowMatrices[casterIndex], faceIndex));
					});
			}
		}

		const dU32 shadowsByteSize = shadowCount * (dU32)sizeof(ShadowData);

		if (!pData->shadowsHandle.IsValid())
			pData->shadowsHandle = graph.CreateBuffer({ .debugName{ L"ShadowsBuffer" }, .memory{ RHI::EBufferMemory::GPU }, .byteSize{ shadowsByteSize }, .byteStride{ sizeof(ShadowData) } });
		else if (graph.GetBuffer(pData->shadowsHandle).GetByteSize() < shadowsByteSize)
			graph.ResizeBuffer(pData->shadowsHandle, shadowsByteSize);

		RenderGraphBuilder& builder = context.AddPass();
		builder.Write(pData->shadowsHandle, RHI::EResourceState::CopyDest);
		context.blackboard.Add<ShadowOutputs>({ .shadowMaps = { pData->activeHandles }, .shadows = pData->shadowsHandle });

		builder.OnExecute([pData](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				const dU32 shadowsByteSize = (dU32)pData->shadows.size() * (dU32)sizeof(ShadowData);
				StagingMemory staging = renderer.AllocateStaging(shadowsByteSize, 16);
				memcpy(staging.pCpuAddress, pData->shadows.data(), shadowsByteSize);
				renderer.GetCurrentFrame().commandList.CopyBufferRegion(context.GetRenderGraph().GetBuffer(pData->shadowsHandle), 0, staging.buffer, staging.offset, shadowsByteSize);
			});
	}

	void Shadow::Destroy(Renderer&, ShadowPassData* pData)
	{
		delete pData;
	}
}
