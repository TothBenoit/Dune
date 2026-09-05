#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Scene/Camera.h"

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
		Device& device = renderer.GetRenderContext()->GetDevice();
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
					{.type = EBindingType::Constant, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::All},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(MaterialData), .visibility = EShaderVisibility::Pixel},
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
		pData->matricesSRVIndex = renderer.GetSRVHeap().GetIndex(pData->matricesSRV);

		return pData;
	}

	void Shadow::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData)
	{
		FrameData& frameData = *context.pFrameData;
		dVector<dU32>& shadowCasters = frameData.lights.shadowCasters;
		pData->activeHandles.clear();
		if (shadowCasters.empty())
			return;

		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		RenderContext& renderContext = *renderer.GetRenderContext();
		Device& device = renderContext.GetDevice();
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < (dU32)shadowCasters.size(); casterIndex++)
		{
			Light& light = frameData.lights.allActive[shadowCasters[casterIndex]];
			light.matrixIndex = casterIndex;
			Descriptor srv = srvHeap.Allocate(1);
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
					pData->cubeShadowHandles.push_back(renderer.CreateTexture(kCubeShadowMapDesc));
				ResourceHandle handle = pData->cubeShadowHandles[cubeShadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT, .dimension = ESRVDimension::TextureCube });
				light.shadowIndex = srvHeap.GetIndex(srv);
				pData->activeHandles.push_back(handle);
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
					pData->shadowHandles.push_back(renderer.CreateTexture(kShadowMapDesc));
				ResourceHandle handle = pData->shadowHandles[shadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT });
				light.shadowIndex = srvHeap.GetIndex(srv);
				pData->activeHandles.push_back(handle);
			}
		}

		for (ResourceHandle handle : pData->activeHandles)
			builder.Write(handle, EResourceState::DepthStencil);

		const dU32 matrixCount = (dU32)shadowCasters.size();
		const dU32 matricesByteSize = matrixCount * (dU32)sizeof(dMatrix4x4);
		if (pData->matrices.size() < matrixCount)
			pData->matrices.resize(matrixCount);

		if (pData->matricesBuffer.GetByteSize() < matricesByteSize)
		{
			if (pData->matricesBuffer.Get())
				frame.buffersToRelease.push(pData->matricesBuffer);
			pData->matricesBuffer.Initialize(device,
				{
					.debugName{ L"ShadowMatricesBuffer" },
					.memory{ EBufferMemory::GPU },
					.byteSize{ matricesByteSize },
					.initialState{ EResourceState::Undefined }
				});
			device.CreateSRV(pData->matricesSRV, pData->matricesBuffer, { .elementCount = matrixCount, .byteStride = sizeof(dMatrix4x4) });
			device.CopyDescriptors(1, pData->matricesSRV.cpuAddress, srvHeap.GetDescriptorAt(pData->matricesSRVIndex).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->matricesHandle == kInvalidResourceHandle)
				pData->matricesHandle = renderer.RegisterBuffer(&pData->matricesBuffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->matricesHandle, &pData->matricesBuffer, EResourceState::Undefined);
		}

		builder.Write(pData->matricesHandle, EResourceState::CopyDest);

		const dVector<Material>& materials = renderContext.GetResourceManager().GetMaterials();
		dVector<dU32>& materialDescriptorCache = pData->materialDescriptorCache;
		materialDescriptorCache.assign(materials.size(), dU32(-1));
	}

	static void RenderDepth(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		ScratchDescriptorHeap& srvHeap = frame.srvHeap;
		RenderContext* pRenderContext = renderer.GetRenderContext();
		ResourceManager& resourceManager = pRenderContext->GetResourceManager();
		Device& device = pRenderContext->GetDevice();

		commandList.SetGraphicsRootSignature(pData->shadowRS);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		FrameData& frameData = *context.pFrameData;

		dU32 currentVariant = dU32(-1);
		for (dU32 drawIdx = 0; drawIdx < (dU32)frameData.drawItems.size() - frameData.blendingMaterialCount; drawIdx++)
		{
			const DrawItem& drawItem = frameData.drawItems[drawIdx];
			Assert(drawItem.materialVariant < Material::kDepthVariantCount);
			const Material& material = resourceManager.GetMaterial(drawItem.materialIdx);
			Assert(material.alphaMode != EAlphaMode::Blend);

			if (currentVariant != drawItem.materialVariant)
			{
				currentVariant = drawItem.materialVariant;
				commandList.SetPipelineState(pData->shadowPSO[currentVariant]);
			}

			if (material.alphaMode == EAlphaMode::Mask)
			{
				MaterialData materialData = material.shaderData;
				if (materialData.albedoIdx != dU32(-1))
				{
					dVector<dU32>& materialDescriptorCache = pData->materialDescriptorCache;
					dU32 materialSRVIdx = materialDescriptorCache[drawItem.materialIdx];
					if (materialSRVIdx == dU32(-1))
					{
						Descriptor materialSRV = srvHeap.Allocate(1);
						materialSRVIdx = srvHeap.GetIndex(materialSRV);
						materialDescriptorCache[drawItem.materialIdx] = materialSRVIdx;
						device.CreateSRV(materialSRV, resourceManager.GetTexture(materialData.albedoIdx));
					}
					materialData.albedoIdx = materialSRVIdx;
				}
				commandList.PushGraphicsConstants(2, &materialData, sizeof(MaterialData));
			}

			InstanceData instanceData;
			instanceData.objectToWorld = drawItem.objectToWorld;
			commandList.PushGraphicsConstants(1, &instanceData, sizeof(InstanceData));
			Mesh& mesh = resourceManager.GetMesh(drawItem.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(drawItem.indexCount, 1, drawItem.indexOffset, drawItem.vertexOffset, 0);
		}
	}

	void Shadow::Execute(RenderPassContext& context, ShadowData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		CommandList& commandList = frame.commandList;
		FrameData& frameData = *context.pFrameData;
		Device& device = renderer.GetRenderContext()->GetDevice();
		BlockDescriptorHeap& dsvHeap = renderer.GetDSVHeap();

		dVector<dU32>& shadowCasters = frameData.lights.shadowCasters;
		dVector<Light>& lights = frameData.lights.allActive;

		Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
		Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
		commandList.SetViewports(1, &viewport);
		commandList.SetScissors(1, &scissor);

		for (dU32 casterIndex = 0; casterIndex < (dU32)shadowCasters.size(); casterIndex++)
		{
			Light& light = lights[shadowCasters[casterIndex]];
			Texture& shadowMap = renderer.GetTexture(pData->activeHandles[casterIndex]);
			dMatrix4x4& lightMatrix = pData->matrices[casterIndex];

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
					DirectX::XMStoreFloat4x4(&faceMatrix, viewMatrices[faceIndex] * projectionMatrix);
					RenderDepth(context, pData, faceMatrix);
				}
				DirectX::XMStoreFloat4x4(&lightMatrix, projectionMatrix);
			}
			else
			{
				device.CreateDSV(dsv, shadowMap, {});
				commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
				commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

				dVec up{ 0.f, 1.f, 0.f, 0.f };
				dVec to{ DirectX::XMLoadFloat3(&light.direction) };
				dVec axis = DirectX::XMVector3Cross(up, to);
				if (DirectX::XMVector3Equal(axis, { 0.0f, 0.0f, 0.0f }))
					up = { 0.f, 0.f, 1.f, 0.f };

				if (light.IsSpot())
				{
					dVec eye{ light.position.x, light.position.y, light.position.z };
					dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
					dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(light.angle * 2.0f, 1.0f, 0.1f, light.range) };
					DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
				}
				else
				{
					float shadowWidth{ 4500.f }; // Hardcoded for sponza
					dVec eye{ 0.f, 0.f, 0.f };
					dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
					dMatrix projectionMatrix{ DirectX::XMMatrixOrthographicLH(shadowWidth, shadowWidth, -shadowWidth, shadowWidth) };
					DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix* projectionMatrix);
				}

				RenderDepth(context, pData, lightMatrix);
			}

			dsvHeap.Free(dsv);
		}

		const dU32 matricesByteSize = (dU32)shadowCasters.size() * (dU32)sizeof(dMatrix4x4);
		Buffer matricesUploadBuffer{};
		matricesUploadBuffer.Initialize(device,
			{
				.debugName{ L"ShadowMatricesUploadBuffer" },
				.memory{ EBufferMemory::CPU },
				.byteSize{ matricesByteSize },
				.initialState{ EResourceState::Undefined }
			});
		void* pMatricesData{ nullptr };
		matricesUploadBuffer.Map(0, matricesByteSize, &pMatricesData);
		memcpy(pMatricesData, pData->matrices.data(), matricesByteSize);
		matricesUploadBuffer.Unmap(0, matricesByteSize);
		commandList.CopyBufferRegion(pData->matricesBuffer, 0, matricesUploadBuffer, 0, matricesByteSize);
		frame.buffersToRelease.push(matricesUploadBuffer);
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
