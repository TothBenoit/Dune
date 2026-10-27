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
		.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1},
		.mipLevels = 1,
		.format = RHI::EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = RHI::EResourceState::DepthStencil
	};

	static const RHI::TextureDesc kCubeShadowMapDesc
	{
		.debugName = L"CubeShadowMap",
		.usage = RHI::ETextureUsage::DepthStencil | RHI::ETextureUsage::ShaderResource,
		.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 6},
		.mipLevels = 1,
		.format = RHI::EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = RHI::EResourceState::DepthStencil
	};

	ShadowData* Shadow::Create(Renderer& renderer)
	{
		ShadowData* pData = new ShadowData();
		pData->matricesSRV = renderer.GetSRVHeap().Allocate();
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

	static void RenderDepth(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		RHI::CommandList& commandList = frame.commandList;
		RHI::ScratchDescriptorHeap& srvHeap = frame.srvHeap;
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

	void Shadow::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData)
	{
		pData->activeHandles.clear();
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
		RHI::ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		dU32 shadowCount = shadowCasters.GetSize();
		dU32 shadowStartIndex = srvHeap.GetIndex(srvHeap.Allocate(shadowCount));

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < shadowCount; casterIndex++)
		{
			const LightData& light = pVisibleSet->lights[shadowCasters[casterIndex]];
			RHI::Descriptor srv = srvHeap.GetDescriptorAt(shadowStartIndex + casterIndex);
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
					pData->cubeShadowHandles.push_back(renderer.CreateTexture(kCubeShadowMapDesc));
				ResourceHandle handle = pData->cubeShadowHandles[cubeShadowIndex++];
				RHI::Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = RHI::EFormat::R32_FLOAT, .dimension = RHI::ESRVDimension::TextureCube });
				pData->activeHandles.push_back(handle);
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
					pData->shadowHandles.push_back(renderer.CreateTexture(kShadowMapDesc));
				ResourceHandle handle = pData->shadowHandles[shadowIndex++];
				RHI::Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = RHI::EFormat::R32_FLOAT });
				pData->activeHandles.push_back(handle);
			}
		}

		for (ResourceHandle handle : pData->activeHandles)
			builder.Write(handle, RHI::EResourceState::DepthStencil);

		const dU32 matrixCount = shadowCount;
		const dU32 matricesByteSize = matrixCount * (dU32)sizeof(dMatrix4x4);

		if (pData->matricesBuffer.GetByteSize() < matricesByteSize)
		{
			if (pData->matricesBuffer.Get())
				frame.buffersToRelease.push_back(pData->matricesBuffer);
			pData->matricesBuffer.Initialize(device,
				{
					.debugName{ L"ShadowMatricesBuffer" },
					.memory{ RHI::EBufferMemory::GPU },
					.byteSize{ matricesByteSize },
					.initialState{ RHI::EResourceState::Undefined }
				});
			device.CreateSRV(pData->matricesSRV, pData->matricesBuffer, { .elementCount = matrixCount, .byteStride = sizeof(dMatrix4x4) });
			device.CopyDescriptors(1, pData->matricesSRV.cpuAddress, context.GetGPUDescriptor(frame, pData->matricesSRV).cpuAddress, RHI::EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->matricesHandle == kInvalidResourceHandle)
				pData->matricesHandle = renderer.RegisterBuffer(&pData->matricesBuffer, RHI::EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->matricesHandle, &pData->matricesBuffer, RHI::EResourceState::Undefined);
		}

		builder.Write(pData->matricesHandle, RHI::EResourceState::CopyDest);
		builder.Read(pMaterialOutputs->buffer, RHI::EResourceState::ShaderResource);
		context.blackboard.Add<ShadowOutputs>({ .shadows = { pData->activeHandles }, .matrices = pData->matricesHandle, .shadowStartIndex = shadowStartIndex, .matricesIndex = context.GetBindlessIndex(pData->matricesSRV) });

		builder.OnExecute([pData](RenderPassContext& context)
			{
				Renderer& renderer = *context.pRenderer;
				Frame& frame = renderer.GetCurrentFrame();
				RHI::CommandList& commandList = frame.commandList;
				RHI::Device& device = *renderer.GetDevice();
				RHI::BlockDescriptorHeap& dsvHeap = renderer.GetDSVHeap();

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
					RHI::Texture& shadowMap = renderer.GetTexture(pData->activeHandles[casterIndex]);
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
							device.CreateDSV(dsv, shadowMap, { .firstArraySlice = faceIndex, .arraySize = 1, .dimension = RHI::EDSVDimension::Texture2DArray });
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

				const dU32 matricesByteSize = shadowCasters.GetSize() * (dU32)sizeof(dMatrix4x4);
				RHI::StagingMemory staging = renderer.AllocateStaging(matricesByteSize, 16);
				memcpy(staging.pCpuAddress, visibleSet.shadowMatrices.GetData(), matricesByteSize);
				commandList.CopyBufferRegion(pData->matricesBuffer, 0, staging.buffer, staging.offset, matricesByteSize);
			});
	}

	void Shadow::Destroy(Renderer& renderer, ShadowData* pData)
	{
		renderer.GetSRVHeap().Free(pData->matricesSRV);
		if (pData->matricesBuffer.Get())
			pData->matricesBuffer.Destroy();
		delete pData;
	}
}
