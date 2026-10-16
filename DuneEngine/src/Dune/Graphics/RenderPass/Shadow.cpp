#include "pch.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	static const TextureDesc kShadowMapDesc
	{
		.debugName = L"ShadowMap",
		.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
		.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1},
		.mipLevels = 1,
		.format = EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = EResourceState::DepthStencil
	};

	static const TextureDesc kCubeShadowMapDesc
	{
		.debugName = L"CubeShadowMap",
		.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
		.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 6},
		.mipLevels = 1,
		.format = EFormat::D32_FLOAT,
		.clearValue = {1.f, 1.f, 1.f, 1.f},
		.initialState = EResourceState::DepthStencil
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
			.depthBias = 10,
			.slopeScaledDepthBias = 4.0f,
			.depthClipEnable = false,
			.depthEnabled = true,
			.depthWrite = true,
			.depthStencilFormat = EFormat::D32_FLOAT,
		};
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
		Device& device = *renderer.GetDevice();
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		dU32 shadowCount = shadowCasters.GetSize();
		dU32 shadowStartIndex = srvHeap.GetIndex(srvHeap.Allocate(shadowCount));

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < shadowCount; casterIndex++)
		{
			const LightData& light = pVisibleSet->lights[shadowCasters[casterIndex]];
			Descriptor srv = srvHeap.GetDescriptorAt(shadowStartIndex + casterIndex);
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
					pData->cubeShadowHandles.push_back(renderer.CreateTexture(kCubeShadowMapDesc));
				ResourceHandle handle = pData->cubeShadowHandles[cubeShadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT, .dimension = ESRVDimension::TextureCube });
				pData->activeHandles.push_back(handle);
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
					pData->shadowHandles.push_back(renderer.CreateTexture(kShadowMapDesc));
				ResourceHandle handle = pData->shadowHandles[shadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT });
				pData->activeHandles.push_back(handle);
			}
		}

		for (ResourceHandle handle : pData->activeHandles)
			builder.Write(handle, EResourceState::DepthStencil);

		const dU32 matrixCount = shadowCount;
		const dU32 matricesByteSize = matrixCount * (dU32)sizeof(dMatrix4x4);

		if (pData->matricesBuffer.GetByteSize() < matricesByteSize)
		{
			if (pData->matricesBuffer.Get())
				frame.buffersToRelease.push_back(pData->matricesBuffer);
			pData->matricesBuffer.Initialize(device,
				{
					.debugName{ L"ShadowMatricesBuffer" },
					.memory{ EBufferMemory::GPU },
					.byteSize{ matricesByteSize },
					.initialState{ EResourceState::Undefined }
				});
			device.CreateSRV(pData->matricesSRV, pData->matricesBuffer, { .elementCount = matrixCount, .byteStride = sizeof(dMatrix4x4) });
			device.CopyDescriptors(1, pData->matricesSRV.cpuAddress, context.GetGPUDescriptor(frame, pData->matricesSRV).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->matricesHandle == kInvalidResourceHandle)
				pData->matricesHandle = renderer.RegisterBuffer(&pData->matricesBuffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->matricesHandle, &pData->matricesBuffer, EResourceState::Undefined);
		}

		builder.Write(pData->matricesHandle, EResourceState::CopyDest);
		builder.Read(pMaterialOutputs->buffer, EResourceState::ShaderResource);
		context.blackboard.Add<ShadowOutputs>({ .shadows = { pData->activeHandles }, .matrices = pData->matricesHandle, .shadowStartIndex = shadowStartIndex, .matricesIndex = context.GetBindlessIndex(pData->matricesSRV) });
	}

	static void RenderDepth(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();

		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

		const FrameData& frameData = *context.pFrameData;
		DepthGlobals globals
		{
			.viewProjectionMatrix = viewProjection,
			.materialBufferIndex =context.blackboard.Get<MaterialOutputs>().bufferIndex,
		};

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

	void Shadow::Execute(RenderPassContext& context, ShadowData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		Device& device = *renderer.GetDevice();
		BlockDescriptorHeap& dsvHeap = renderer.GetDSVHeap();

		const VisibleSet& visibleSet = context.blackboard.Get<VisibleSet>();
		const dSpan<const dU32> shadowCasters = visibleSet.shadowCasters;
		const dSpan<const LightData> lights = visibleSet.lights;

		Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
		Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);

		for (dU32 casterIndex = 0; casterIndex < shadowCasters.GetSize(); casterIndex++)
		{
			const LightData& light = lights[shadowCasters[casterIndex]];
			Texture& shadowMap = renderer.GetTexture(pData->activeHandles[casterIndex]);
			const dMatrix4x4& lightMatrix = visibleSet.shadowMatrices[casterIndex];

			Descriptor dsv = dsvHeap.Allocate();
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
					device.CreateDSV(dsv, shadowMap, { .firstArraySlice = faceIndex, .arraySize = 1, .dimension = EDSVDimension::Texture2DArray });
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
		StagingMemory staging = renderer.AllocateStaging(matricesByteSize, 16);
		memcpy(staging.pCpuAddress, visibleSet.shadowMatrices.GetData(), matricesByteSize);
		commandList.CopyBufferRegion(pData->matricesBuffer, 0, staging.buffer, staging.offset, matricesByteSize);
	}

	void Shadow::Destroy(Renderer& renderer, ShadowData* pData)
	{
		renderer.GetSRVHeap().Free(pData->matricesSRV);
		if (pData->matricesBuffer.Get())
			pData->matricesBuffer.Destroy();
		delete pData;
	}
}
