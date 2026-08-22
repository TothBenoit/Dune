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
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	RenderPassDesc Shadow::GetDesc()
	{
		RenderPassDesc desc
		{
			.name = "Shadow",
			.writes =
			{
				{ .id = EResourceTag::ShadowMaps, .state = EResourceState::DepthStencil },
				{ .id = EResourceTag::LightMatrices, .state = EResourceState::CopyDest },
			},
		};
		return desc;
	}

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

		return pData;
	}

	void Render(RenderPassContext& context, ShadowData* pData, const dMatrix4x4& viewProjection)
	{
		Renderer& renderer = *context.pRenderer;
		CommandList& commandList = *context.pCommandList;
		Scene& scene = *context.pScene;
		ResourceManager& resourceManager = renderer.GetRenderContext()->GetResourceManager();

		commandList.SetGraphicsRootSignature(pData->shadowRS);
		commandList.SetPipelineState(pData->shadowPSO);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.PushGraphicsConstants(0, &viewProjection, sizeof(dMatrix4x4));

		const entt::registry& kRegistry = scene.registry;
		kRegistry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
			{
				InstanceData instance;
				DirectX::XMStoreFloat4x4(&instance.modelMatrix,
					DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
					DirectX::XMMatrixRotationQuaternion(transform.rotation) *
					DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
				);

				commandList.PushGraphicsConstants(1, &instance.modelMatrix, sizeof(InstanceData));
				Mesh& mesh = resourceManager.GetMesh(renderData.meshIdx);
				commandList.BindIndexBuffer(mesh.GetIndexBuffer(), mesh.IsIndex32bits());
				commandList.BindVertexBuffer(mesh.GetVertexBuffer(), mesh.GetVertexByteStride());
				commandList.DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
			});
	}

	void Shadow::Execute(RenderPassContext& context, ShadowData* pData)
	{
		Renderer& renderer = *context.pRenderer;
		CommandList& commandList = *context.pCommandList;
		Scene& scene = *context.pScene;
		Frame& frame = *context.pFrame;
		Device& device = renderer.GetRenderContext()->GetDevice();
		ResourceManager& resourceManager = renderer.GetRenderContext()->GetResourceManager();
		BlockDescriptorHeap& srvHeap = renderer.GetSRVHeap();
		BlockDescriptorHeap& dsvHeap = renderer.GetDSVHeap();
		Barrier& barrier = *context.pBarrier;

		LightMatrices& lightMatrices = *(LightMatrices*)renderer.GetSharedResource(EResourceTag::LightMatrices);
		ShadowMaps& shadowMaps = *(ShadowMaps*)renderer.GetSharedResource(EResourceTag::ShadowMaps);

		{
			dVector<dU32>& shadowCasters = renderer.GetShadowCastingLightsIndex();
			dVector<Light>& lights = renderer.GetLights();
			dU32 shadowCount = (dU32)shadowCasters.size();
			if (shadowCount > 0)
			{
				Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
				Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
				frame.commandList.SetViewports(1, &viewport);
				frame.commandList.SetScissors(1, &scissor);

				dU32 cubeShadowIndex{ 0 };
				dU32 shadowIndex{ 0 };
				dU32 matrixIndex{ 0 };

				for (dU32 lightIndex : shadowCasters)
				{
					Light& light = lights[lightIndex];
					Descriptor dsv = dsvHeap.Allocate();
					Descriptor srv = frame.srvHeap.Allocate(1);

					if (light.IsPoint() == ELightType::Point)
					{
						if (shadowMaps.cubeShadows.size() <= cubeShadowIndex)
							shadowMaps.cubeShadows.emplace_back();
						if (lightMatrices.matrices.size() <= matrixIndex)
							lightMatrices.matrices.emplace_back();
						Texture& shadowMap = shadowMaps.cubeShadows[cubeShadowIndex++];
						dMatrix4x4& lightMatrix = lightMatrices.matrices[matrixIndex];
						light.matrixIndex = matrixIndex++;
						if (!shadowMap.Get())
						{
							shadowMap.Initialize(device,
								{
									.debugName = L"CubeShadowMap",
									.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
									.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 6},
									.format = EFormat::D32_FLOAT,
									.clearValue = {1.f, 1.f, 1.f, 1.f},
									.initialState = EResourceState::DepthStencil
								});
						}
						else
						{
							barrier.PushTransition(shadowMap.Get(), EResourceState::ShaderResource, EResourceState::DepthStencil);
							frame.commandList.Transition(barrier);
							barrier.Reset();
						}

						device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT, .dimension = ESRVDimension::TextureCube });
						light.shadowIndex = frame.srvHeap.GetIndex(srv);

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
							frame.commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
							frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
							DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrices[faceIndex] * projectionMatrix);
							Render(context, pData, lightMatrix);
						}
						barrier.PushTransition(shadowMap.Get(), EResourceState::DepthStencil, EResourceState::ShaderResource);
						frame.commandList.Transition(barrier);
						barrier.Reset();
						DirectX::XMStoreFloat4x4(&lightMatrix, projectionMatrix);
					}
					else
					{
						if (shadowMaps.shadows.size() <= shadowIndex)
							shadowMaps.shadows.emplace_back();
						if (lightMatrices.matrices.size() <= matrixIndex)
							lightMatrices.matrices.emplace_back();
						Texture& shadowMap = shadowMaps.shadows[shadowIndex++];
						dMatrix4x4& lightMatrix = lightMatrices.matrices[matrixIndex];
						light.matrixIndex = matrixIndex++;
						if (!shadowMap.Get())
						{
							shadowMap.Initialize(device,
								{
									.debugName = L"ShadowMap",
									.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
									.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1},
									.format = EFormat::D32_FLOAT,
									.clearValue = {1.f, 1.f, 1.f, 1.f},
									.initialState = EResourceState::DepthStencil
								});
						}
						else
						{
							barrier.PushTransition(shadowMap.Get(), EResourceState::ShaderResource, EResourceState::DepthStencil);
							frame.commandList.Transition(barrier);
							barrier.Reset();
						}

						device.CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT });
						device.CreateDSV(dsv, shadowMap, {});
						frame.commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
						frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

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

						Render(context, pData, lightMatrix);
						light.shadowIndex = frame.srvHeap.GetIndex(srv);
						barrier.PushTransition(shadowMap.Get(), EResourceState::DepthStencil, EResourceState::ShaderResource);
						frame.commandList.Transition(barrier);
						barrier.Reset();
					}

					dsvHeap.Free(dsv);
				}

					dU32 matricesByteSize = (dU32)((matrixIndex) * sizeof(dMatrix4x4));
					if (lightMatrices.buffer.GetByteSize() < matricesByteSize)
					{
						if (lightMatrices.buffer.Get())
							frame.buffersToRelease.push(lightMatrices.buffer);
						lightMatrices.buffer.Initialize(device,
							{
								.debugName{ L"LightMatricesBuffer" },
								.memory{ EBufferMemory::GPU },
								.byteSize{ matricesByteSize  },
								.initialState{ EResourceState::Undefined }
							});
						device.CreateSRV(lightMatrices.srv, lightMatrices.buffer, { .elementCount = matrixIndex, .byteStride = sizeof(dMatrix4x4) });
						device.CopyDescriptors(1, lightMatrices.srv.cpuAddress, frame.srvHeap.GetDescriptorAt(srvHeap.GetIndex(lightMatrices.srv)).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
					}

					Buffer matricesUploadBuffer{};
					matricesUploadBuffer.Initialize(device,
						{
							.debugName{ L"MatricesUploadBuffer" },
							.memory{ EBufferMemory::CPU },
							.byteSize{ matricesByteSize  },
							.initialState{ EResourceState::Undefined }
						});
					void* pMatricesData{ nullptr };
					matricesUploadBuffer.Map(0, matricesByteSize, &pMatricesData);
					memcpy(pMatricesData, lightMatrices.matrices.data(), matricesByteSize);
					matricesUploadBuffer.Unmap(0, matricesByteSize);
					frame.commandList.CopyBufferRegion(lightMatrices.buffer, 0, matricesUploadBuffer, 0, matricesByteSize);
					frame.buffersToRelease.push(matricesUploadBuffer);
					barrier.PushTransition(lightMatrices.buffer.Get(), EResourceState::CopyDest, EResourceState::ShaderResource);
			}
		}
	}

	void Shadow::Destroy(Renderer&, ShadowData* pData)
	{
		pData->shadowPSO.Destroy();
		pData->shadowRS.Destroy();
		delete pData;
	}
}
