#include "pch.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"
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

		Shader shadowVS;
		shadowVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\DepthOnly.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		pData->shadowRS.Initialize(device,
			{
				.layout =
				{
					{.type = EBindingType::Constant, .byteSize = sizeof(dMatrix4x4), .visibility = EShaderVisibility::Vertex},
					{.type = EBindingType::Constant, .byteSize = sizeof(InstanceData), .visibility = EShaderVisibility::Vertex},
				},
				.bAllowInputLayout = true,
			});

		pData->shadowPSO.Initialize(device,
			{
				.pVertexShader = &shadowVS,

				.pRootSignature = &pData->shadowRS,
				.inputLayout =
				{
					VertexInput {.pName = "POSITION", .index = 0, .format = EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
				},
				.rasterizerState = {.depthBias = 10, .slopeScaledDepthBias = 4, .bDepthClipEnable = false},
				.depthStencilState = {.bDepthEnabled = true, .bDepthWrite = true },
				.depthStencilFormat = EFormat::D32_FLOAT,
			}
			);
		shadowVS.Destroy();

		pData->matricesSRV = renderer.GetSRVHeap().Allocate();
		pData->matricesSRVIndex = renderer.GetSRVHeap().GetIndex(pData->matricesSRV);

		return pData;
	}

	void Shadow::Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		Frame& frame = renderer.GetCurrentFrame();
		FrameData& frameData = *context.pFrameData;
		dVector<dU32>& shadowCasters = frameData.lights.shadowCasters;
		Device& device = renderer.GetRenderContext()->GetDevice();

		pData->activeHandles.clear();
		if (shadowCasters.empty())
			return;

		dU32 cubeShadowIndex{ 0 };
		dU32 shadowIndex{ 0 };
		for (dU32 casterIndex = 0; casterIndex < (dU32)shadowCasters.size(); casterIndex++)
		{
			Light& light = frameData.lights.allActive[shadowCasters[casterIndex]];
			light.matrixIndex = casterIndex;
			Descriptor srv = frame.srvHeap.Allocate(1);
			if (light.IsPoint())
			{
				if (pData->cubeShadowHandles.size() <= cubeShadowIndex)
					pData->cubeShadowHandles.push_back(renderer.CreateTexture(kCubeShadowMapDesc));
				ResourceHandle handle = pData->cubeShadowHandles[cubeShadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT, .dimension = ESRVDimension::TextureCube });
				light.shadowIndex = frame.srvHeap.GetIndex(srv);
				pData->activeHandles.push_back(handle);
			}
			else
			{
				if (pData->shadowHandles.size() <= shadowIndex)
					pData->shadowHandles.push_back(renderer.CreateTexture(kShadowMapDesc));
				ResourceHandle handle = pData->shadowHandles[shadowIndex++];
				Texture& shadowMap = renderer.GetTexture(handle);
				device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT });
				light.shadowIndex = frame.srvHeap.GetIndex(srv);
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
			device.CopyDescriptors(1, pData->matricesSRV.cpuAddress, frame.srvHeap.GetDescriptorAt(pData->matricesSRVIndex).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);

			if (pData->matricesHandle == kInvalidResourceHandle)
				pData->matricesHandle = renderer.RegisterBuffer(&pData->matricesBuffer, EResourceState::Undefined);
			else
				renderer.SetPhysicalResource(pData->matricesHandle, &pData->matricesBuffer, EResourceState::Undefined);
		}

		builder.Write(pData->matricesHandle, EResourceState::CopyDest);
	}

	static void RenderDepth(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		CommandList& commandList = renderer.GetCurrentFrame().commandList;
		ResourceManager& resourceManager = renderer.GetRenderContext()->GetResourceManager();

		commandList.SetGraphicsRootSignature(pData->shadowRS);
		commandList.SetPipelineState(pData->shadowPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		for( const DrawItem& drawItem : context.pFrameData->drawItems)
		{
			InstanceData data;
			data.objectToWorld = drawItem.objectToWorld;
			commandList.PushGraphicsConstants(1, &data, sizeof(InstanceData));
			Mesh& mesh = resourceManager.GetMesh(drawItem.data.meshIdx);
			commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
			commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
			commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
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
		pData->shadowPSO.Destroy();
		pData->shadowRS.Destroy();
		delete pData;
	}
}
