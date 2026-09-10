#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"
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
		Device& device = *renderer.GetDevice();
		ShadowData* pData = new ShadowData();

		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };
		const wchar_t* maskedArgs[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug", L"-D", L"ALPHA_MASK" };

		dWString shaderPath = StringUtils::ToWide(FileSystem::ResolvePath("engine://Shaders/DepthOnly.hlsl"));
		ShaderDesc shaderDesc
		{
			.stage = EShaderStage::Vertex,
			.filePath = shaderPath.c_str(),
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		};

		Shader shadowVS[2];
		shadowVS[0].Initialize(shaderDesc);
		shaderDesc.args = maskedArgs;
		shaderDesc.argsCount = _countof(maskedArgs);
		shadowVS[1].Initialize(shaderDesc);

		Shader shadowMaskedPS;
		shaderDesc.stage = EShaderStage::Pixel;
		shaderDesc.entryFunc = L"PSMain";
		shadowMaskedPS.Initialize(shaderDesc);

		pData->shadowRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(DepthGlobals),   .visibility = EShaderVisibility::All},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(MaterialIndex),         .visibility = EShaderVisibility::Pixel},
				},
				.allowInputLayout = true,
				.allowSRVHeapIndexing = true,
			});

		VertexInput maskedVertexInputs[]
		{
			VertexInput{.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
			VertexInput{.pName = "UV", .index = 0, .format = EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 40, .isPerInstance = false },
		};

		VertexInput vertexInputs[]
		{
			VertexInput{.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .isPerInstance = false },
		};

		for (dU32 variant = 0; variant < Material::kDepthVariantCount; variant++)
		{
			bool isMasked = Material::GetAlphaMode(variant) == EAlphaMode::Mask;
			dSpan<VertexInput> inputLayout = isMasked ? dSpan<VertexInput>(maskedVertexInputs) : dSpan<VertexInput>(vertexInputs);
			pData->shadowPSO[variant].Initialize(device,
			{
				.pVertexShader = &shadowVS[isMasked ? 1 : 0],
				.pPixelShader = isMasked ? &shadowMaskedPS : nullptr,
				.pRootSignature = &pData->shadowRS,
				.inputLayout = inputLayout,
				.rasterizerState = {.depthBias = 10, .slopeScaledDepthBias = 4.0f, .cullingMode = Material::IsDoubleSided(variant) ? ECullingMode::None : ECullingMode::Back, .depthClipEnable = false },
				.depthStencilState = {.depthEnabled = true, .depthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			});
		}

		shadowMaskedPS.Destroy();
		for (Shader& vs : shadowVS)
			vs.Destroy();

		pData->matricesSRV = renderer.GetSRVHeap().Allocate();
		pData->matricesPersistentSRVIndex = renderer.GetSRVHeap().GetIndex(pData->matricesSRV);

		return pData;
	}

	void Shadow::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData)
	{
		const FrameData& frameData = *context.pFrameData;
		const dVector<dU32>& shadowCasters = frameData.lights.shadowCasters;
		pData->activeHandles.clear();
		if (shadowCasters.empty())
			return;

		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		Device& device = *renderer.GetDevice();
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		dU32 shadowCount = (dU32)shadowCasters.size();
		dU32 shadowStartIndex = pData->shadowStartIndex = srvHeap.GetIndex(srvHeap.Allocate(shadowCount));

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < shadowCount; casterIndex++)
		{
			const Light& light = frameData.lights.allActive[shadowCasters[casterIndex]];
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

		const dU32 matrixCount = (dU32)shadowCasters.size();
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
			device.CopyDescriptors(1, pData->matricesSRV.cpuAddress, srvHeap.GetDescriptorAt(pData->matricesPersistentSRVIndex + frameData.sharedSRVHeapCapacity).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->matricesHandle == kInvalidResourceHandle)
				pData->matricesHandle = renderer.RegisterBuffer(&pData->matricesBuffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->matricesHandle, &pData->matricesBuffer, EResourceState::Undefined);
		}

		builder.Write(pData->matricesHandle, EResourceState::CopyDest);
		builder.Read(renderer.Get<MaterialUpload>()->handle, EResourceState::ShaderResource);
	}

	static void RenderDepth(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		Device& device = *renderer.GetDevice();

		commandList.SetGraphicsRootSignature(pData->shadowRS);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);

		const FrameData& frameData = *context.pFrameData;
		DepthGlobals globals
		{
			.viewProjectionMatrix = viewProjection,
			.materialBufferIndex = renderer.GetSRVHeap().GetIndex(renderer.Get<MaterialUpload>()->srv) + frameData.sharedSRVHeapCapacity
		};
		commandList.PushGraphicsConstants(0, &globals, sizeof(DepthGlobals));

		dU32 currentVariant = dU32(-1);
		for (dU32 drawIdx = 0; drawIdx < (dU32)frameData.drawItems.size() - frameData.blendDrawCount; drawIdx++)
		{
			const DrawItem& drawItem = frameData.drawItems[drawIdx];
			EAlphaMode alphaMode = Material::GetAlphaMode(drawItem.materialVariant);
			Assert(drawItem.materialVariant < Material::kDepthVariantCount);
			Assert(alphaMode != EAlphaMode::Blend);

			if (currentVariant != drawItem.materialVariant)
			{
				currentVariant = drawItem.materialVariant;
				commandList.SetPipelineState(pData->shadowPSO[currentVariant]);
			}

			if (alphaMode == EAlphaMode::Mask)
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
		const FrameData& frameData = *context.pFrameData;
		Device& device = *renderer.GetDevice();
		BlockDescriptorHeap& dsvHeap = renderer.GetDSVHeap();

		const dVector<dU32>& shadowCasters = frameData.lights.shadowCasters;
		const dVector<Light>& lights = frameData.lights.allActive;

		Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
		Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);

		for (dU32 casterIndex = 0; casterIndex < (dU32)shadowCasters.size(); casterIndex++)
		{
			const Light& light = lights[shadowCasters[casterIndex]];
			Texture& shadowMap = renderer.GetTexture(pData->activeHandles[casterIndex]);
			const dMatrix4x4& lightMatrix = frameData.lights.shadowMatrices[casterIndex];

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

		void* pMappedData{ nullptr };
		const dU32 matricesByteSize = (dU32)shadowCasters.size() * (dU32)sizeof(dMatrix4x4);
		if (frame.uploadOffset + matricesByteSize < frame.uploadBuffer.GetByteSize())
		{
			pMappedData = (dU8*)frame.pUploadAddress + frame.uploadOffset;
			memcpy(pMappedData, frameData.lights.shadowMatrices.data(), matricesByteSize);
			frame.commandList.CopyBufferRegion(pData->matricesBuffer, 0, frame.uploadBuffer, frame.uploadOffset, matricesByteSize);
			frame.uploadOffset += matricesByteSize;
		}
		else
		{
			Buffer uploadBuffer{};
			uploadBuffer.Initialize(device, { .debugName{ L"ShadowMatricesUploadBuffer" }, .byteSize{ matricesByteSize } });
			uploadBuffer.Map(0, matricesByteSize, &pMappedData);
			memcpy(pMappedData, frameData.lights.shadowMatrices.data(), matricesByteSize);
			uploadBuffer.Unmap(0, matricesByteSize);
			frame.commandList.CopyBufferRegion(pData->matricesBuffer, 0, uploadBuffer, 0, matricesByteSize);
			frame.buffersToRelease.push_back(uploadBuffer);
		}
	}

	void Shadow::Destroy(Renderer& renderer, ShadowData* pData)
	{
		renderer.GetSRVHeap().Free(pData->matricesSRV);
		if (pData->matricesBuffer.Get())
			pData->matricesBuffer.Destroy();
		for (PipelineState& pso : pData->shadowPSO)
			pso.Destroy();
		pData->shadowRS.Destroy();
		delete pData;
	}
}
