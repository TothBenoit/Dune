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
		ShadowPassData* pData = new ShadowPassData();
		pData->shadowsSRV = renderer.GetSRVAllocator().Allocate();
		return pData;
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

	static void RenderDepth(RenderPassContext& context, ShadowPassData* pData, const dMatrix4x4& viewProjection)
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
			.materialBufferIndex =context.blackboard.Get<MaterialOutputs>().bufferIndex,
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

	void Shadow::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowPassData* pData)
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

		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		RHI::Device& device = *renderer.GetDevice();
		dU32 shadowCount = shadowCasters.GetSize();

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < shadowCount; casterIndex++)
		{
			const LightData& light = pVisibleSet->lights[shadowCasters[casterIndex]];
			RHI::Descriptor srv;
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
				{
					const RGTexture handle = renderer.GetRenderGraph().CreateTexture(kCubeShadowMapDesc);
					const RHI::Descriptor shadowSRV = renderer.GetSRVAllocator().Allocate();
					device.CreateSRV(shadowSRV, renderer.GetRenderGraph().GetTexture(handle), { .format = RHI::EFormat::R32_FLOAT, .dimension = RHI::ETextureDimension::TextureCube });
					pData->cubeShadowHandles.push_back(handle);
					pData->cubeShadowSRVs.push_back(shadowSRV);
				}
				pData->activeHandles.push_back(pData->cubeShadowHandles[cubeShadowIndex]);
				srv = pData->cubeShadowSRVs[cubeShadowIndex++];
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
				{
					const RGTexture handle = renderer.GetRenderGraph().CreateTexture(kShadowMapDesc);
					const RHI::Descriptor shadowSRV = renderer.GetSRVAllocator().Allocate();
					device.CreateSRV(shadowSRV, renderer.GetRenderGraph().GetTexture(handle), { .format = RHI::EFormat::R32_FLOAT });
					pData->shadowHandles.push_back(handle);
					pData->shadowSRVs.push_back(shadowSRV);
				}
				pData->activeHandles.push_back(pData->shadowHandles[shadowIndex]);
				srv = pData->shadowSRVs[shadowIndex++];
			}
			pData->shadows.push_back({ .shadowMatrix = pVisibleSet->shadowMatrices[casterIndex], .shadowIndex = context.GetBindlessIndex(srv) });
		}

		for (RGTexture handle : pData->activeHandles)
			builder.Write(handle, RHI::EResourceState::DepthStencil);

		const dU32 shadowsByteSize = shadowCount * (dU32)sizeof(ShadowData);

		if (pData->shadowsBuffer.GetByteSize() < shadowsByteSize)
		{
			if (pData->shadowsBuffer.Get())
			{
				frame.buffersToRelease.push_back(pData->shadowsBuffer);
				renderer.ReleaseSRV(pData->shadowsSRV);
				pData->shadowsSRV = renderer.GetSRVAllocator().Allocate();
			}
			pData->shadowsBuffer.Initialize(device,
				{
					.debugName{ L"ShadowsBuffer" },
					.memory{ RHI::EBufferMemory::GPU },
					.byteSize{ shadowsByteSize },
					.initialState{ RHI::EResourceState::Undefined }
				});
			device.CreateSRV(pData->shadowsSRV, pData->shadowsBuffer, { .elementCount = shadowCount, .byteStride = sizeof(ShadowData) });

			if (!pData->shadowsHandle.IsValid())
				pData->shadowsHandle = renderer.GetRenderGraph().RegisterBuffer(&pData->shadowsBuffer, RHI::EResourceState::Undefined);
			else
				renderer.GetRenderGraph().SetPhysicalResource(pData->shadowsHandle, &pData->shadowsBuffer, RHI::EResourceState::Undefined);
		}

		builder.Write(pData->shadowsHandle, RHI::EResourceState::CopyDest);
		builder.Read(pMaterialOutputs->buffer, RHI::EResourceState::ShaderResource);
		context.blackboard.Add<ShadowOutputs>({ .shadowMaps = { pData->activeHandles }, .shadows = pData->shadowsHandle, .shadowsIndex = context.GetBindlessIndex(pData->shadowsSRV) });

		builder.OnExecute([pData](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				RHI::CommandList& commandList = frame.commandList;
				RHI::Device& device = *renderer.GetDevice();
				DescriptorAllocator& dsvHeap = renderer.GetDSVAllocator();

				const VisibleSet& visibleSet = context.blackboard.Get<VisibleSet>();
				const dSpan<const dU32> shadowCasters = visibleSet.shadowCasters;
				const dSpan<const LightData> lights = visibleSet.lights;

				RHI::Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
				RHI::Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
				commandList.SetViewports(1, &viewport);
				commandList.SetScissors(1, &scissor);

				for (dU32 casterIndex = 0; casterIndex < shadowCasters.GetSize(); casterIndex++)
				{
					const LightData& light = lights[shadowCasters[casterIndex]];
					RHI::Texture& shadowMap = renderer.GetRenderGraph().GetTexture(pData->activeHandles[casterIndex]);
					const dMatrix4x4& lightMatrix = visibleSet.shadowMatrices[casterIndex];

					RHI::Descriptor dsv = dsvHeap.Allocate();
					if (light.IsPoint())
					{
						dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.f), 1.0f, 0.1f, light.range) };
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

						for (dU32 faceIndex = 0; faceIndex < 6; faceIndex++)
						{
							device.CreateDSV(dsv, shadowMap, { .firstArraySlice = faceIndex });
							commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
							commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
							dMatrix4x4 faceMatrix;
							DirectX::XMStoreFloat4x4(&faceMatrix, viewMatrices[faceIndex] * DirectX::XMLoadFloat4x4(&lightMatrix));
							RenderDepth(context, pData, faceMatrix);
						}
					}
					else
					{
						device.CreateDSV(dsv, shadowMap, {});
						commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
						commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
						RenderDepth(context, pData, lightMatrix);
					}

					dsvHeap.Free(dsv);
				}

				const dU32 shadowsByteSize = (dU32)pData->shadows.size() * (dU32)sizeof(ShadowData);
				StagingMemory staging = renderer.AllocateStaging(shadowsByteSize, 16);
				memcpy(staging.pCpuAddress, pData->shadows.data(), shadowsByteSize);
				commandList.CopyBufferRegion(pData->shadowsBuffer, 0, staging.buffer, staging.offset, shadowsByteSize);
			});
	}

	void Shadow::Destroy(Renderer& renderer, ShadowPassData* pData)
	{
		DescriptorAllocator& srvAllocator = renderer.GetSRVAllocator();
		for (RHI::Descriptor srv : pData->shadowSRVs)
			srvAllocator.Free(srv);
		for (RHI::Descriptor srv : pData->cubeShadowSRVs)
			srvAllocator.Free(srv);
		srvAllocator.Free(pData->shadowsSRV);
		if (pData->shadowsBuffer.Get())
			pData->shadowsBuffer.Destroy();
		delete pData;
	}
}
