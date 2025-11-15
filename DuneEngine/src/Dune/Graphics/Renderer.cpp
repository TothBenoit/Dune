#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/ImGUIWrapper.h"
#include <Dune/Graphics/Shaders/ShaderTypes.h>
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx12.h>

namespace Dune::Graphics
{
	void Renderer::Initialize(Device& device, Window& window)
	{
		m_pDevice = &device;
		m_pWindow = &window;

		dU32 width = m_pWindow->GetWidth();
		dU32 height = m_pWindow->GetHeight();
		m_depthBuffer.Initialize( m_pDevice, 
			{
				.debugName = L"DepthBuffer",
				.usage = ETextureUsage::DepthStencil,
				.dimensions = { width, height, 1 },
				.format = EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = EResourceState::DepthStencil
			});

		TextureDesc colorTargetDesc
		{
			.debugName = L"ColorTarget",
			.usage{ ETextureUsage::RenderTarget | ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ EFormat::R16G16B16A16_FLOAT },
			.initialState{ EResourceState::ShaderResource },
		};
		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(m_pDevice, ECommandType::Direct);
			frame.commandList.Initialize(m_pDevice, ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
			frame.colorTarget.Initialize(m_pDevice, colorTargetDesc);
			frame.srvHeap.Initialize(m_pDevice, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = 4096, .isShaderVisible = true });
			frame.samplerHeap.Initialize(m_pDevice, { .type = EDescriptorHeapType::Sampler, .capacity = 64, .isShaderVisible = true });
		}

		m_barrier.Initialize(16);
		m_fence.Initialize(m_pDevice, 0);
		m_commandQueue.Initialize(m_pDevice, ECommandType::Direct);
		m_swapchain.Initialize(m_pDevice, m_pWindow, &m_commandQueue, { .latency = 3 });

		DescriptorHeapDesc heapDesc { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = 64, .isShaderVisible = true };
		m_srvImGuiHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.isShaderVisible = false;
		m_srvHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.type = EDescriptorHeapType::RTV;
		m_rtvHeap.Initialize(m_pDevice, heapDesc);

		heapDesc.type = EDescriptorHeapType::DSV;
		m_dsvHeap.Initialize(m_pDevice, heapDesc);

		for (dU32 i = 0; i < 3; i++)
		{
			Frame& frame = m_frames[i];
			frame.backBufferRTV = m_rtvHeap.Allocate();
			frame.colorTargetRTV = m_rtvHeap.Allocate();
			frame.colorTargetSRV = m_srvHeap.Allocate();
			m_pDevice->CreateRTV(frame.backBufferRTV, m_swapchain.GetBackBuffer(i), {});
			m_pDevice->CreateRTV(frame.colorTargetRTV, frame.colorTarget, {});
			m_pDevice->CreateSRV(frame.colorTargetSRV, frame.colorTarget);
		}

		m_depthBufferDSV = m_dsvHeap.Allocate();
		m_pDevice->CreateDSV(m_depthBufferDSV, m_depthBuffer, {});

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		m_forwardPass.Initialize(m_pDevice);
		m_depthPrepass.Initialize(m_pDevice);
		m_shadowPass.Initialize(m_pDevice);
		m_tonemappingPass.Initialize(m_pDevice);

		m_lightsSRV = m_srvHeap.Allocate();
		m_lightMatricesSRV = m_srvHeap.Allocate();
	}

	void Renderer::Destroy()
	{
		for (dU32 i = 0; i < 3; i++) 
		{
			Frame& frame = m_frames[i];
			while (!frame.buffersToRelease.empty())
			{
				frame.buffersToRelease.front().Destroy();
				frame.buffersToRelease.pop();
			}
			WaitForFrame(frame);
			m_rtvHeap.Free(frame.backBufferRTV);
			m_rtvHeap.Free(frame.colorTargetRTV);
			m_srvHeap.Free(frame.colorTargetSRV);
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
			frame.colorTarget.Destroy();
			frame.srvHeap.Destroy();
			frame.samplerHeap.Destroy();
		}
		m_dsvHeap.Free(m_depthBufferDSV);
		m_srvHeap.Free(m_lightsSRV);
		m_srvHeap.Free(m_lightMatricesSRV);

		for (Texture& shadow : m_shadowMaps)
			shadow.Destroy();
		for (Texture& shadow : m_cubeShadowMaps)
			shadow.Destroy();
		
		m_forwardPass.Destroy();
		m_depthPrepass.Destroy();
		m_shadowPass.Destroy();
		m_tonemappingPass.Destroy();

		m_srvHeap.Destroy();
		m_srvImGuiHeap.Destroy();
		m_rtvHeap.Destroy();
		m_dsvHeap.Destroy();
		m_barrier.Destroy();
		m_depthBuffer.Destroy();
		m_commandQueue.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();

		if (m_lightBuffer.Get())
			m_lightBuffer.Destroy();

		if (m_lightMatricesBuffer.Get())
			m_lightMatricesBuffer.Destroy();
	}

	void Renderer::OnResize(dU32 width, dU32 height) 
	{
		TextureDesc colorTargetDesc
		{
			.debugName = L"ColorTarget",
			.usage{ ETextureUsage::RenderTarget | ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ EFormat::R16G16B16A16_FLOAT },
			.initialState{ EResourceState::ShaderResource },
		};

		for (Frame& f : m_frames)
		{
			WaitForFrame(f);
			f.colorTarget.Destroy();
			f.colorTarget.Initialize(m_pDevice, colorTargetDesc);
			m_pDevice->CreateSRV(f.colorTargetSRV, f.colorTarget);
			m_pDevice->CreateRTV(f.colorTargetRTV, f.colorTarget, {});
		}

		m_swapchain.Resize(width, height);
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < 3; i++)
			m_pDevice->CreateRTV(m_frames[i].backBufferRTV, m_swapchain.GetBackBuffer(i), {});

		m_depthBuffer.Destroy();
		m_depthBuffer.Initialize(m_pDevice, 
			{
				.debugName = L"DepthBuffer",
				.usage = ETextureUsage::DepthStencil,
				.dimensions = { width, height, 1 },
				.format = EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = EResourceState::DepthStencil
			});
		m_pDevice->CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
	}

	void Renderer::WaitForFrame(const Frame& frame)
	{
		const dU64 fenceValue{ frame.fenceValue };
		if ( m_fence.GetValue() < fenceValue )
			m_fence.Wait(fenceValue);
	}

	void Renderer::Render(Scene& scene, Camera& camera)
	{
		Frame& frame = m_frames[m_frameIndex];
		WaitForFrame(frame);
		while (!frame.buffersToRelease.empty())
		{
			frame.buffersToRelease.front().Destroy();
			frame.buffersToRelease.pop();
		}
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(frame.srvHeap, frame.samplerHeap);
		frame.srvHeap.Reset();
		frame.samplerHeap.Reset();

		m_pDevice->CopyDescriptors(m_srvHeap.GetCapacity(), m_srvHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress(), EDescriptorHeapType::SRV_CBV_UAV);
		frame.srvHeap.Allocate(m_srvHeap.GetCapacity());

		ForwardGlobals globals;
		ComputeViewProjectionMatrix(camera, nullptr, nullptr, &globals.viewProjectionMatrix);
		globals.cameraPosition = camera.position;

		const entt::registry& kRegistry = scene.registry;
		{
			Buffer uploadBuffer{};
			auto view = kRegistry.view<const Dune::Light>();
			dU32 lightCount = (dU32)view.size();
			if (lightCount > 0)
			{
				dU32 lightByteSize = (dU32)((lightCount) * sizeof(Light));
				if (m_lightBuffer.GetByteSize() < lightByteSize)
				{
					if (m_lightBuffer.Get())
						frame.buffersToRelease.push(m_lightBuffer);
					m_lightBuffer.Initialize(m_pDevice,
						{
							.debugName{ L"LightBuffer" },
							.usage{ EBufferUsage::Constant },
							.memory{ EBufferMemory::GPU },
							.byteSize{ lightByteSize  },
							.byteStride { sizeof(Light) },
							.initialState{ EResourceState::Undefined }
						});
					m_pDevice->CreateSRV(m_lightsSRV, m_lightBuffer);
					m_pDevice->CopyDescriptors(1, m_lightsSRV.cpuAddress, frame.srvHeap.GetDescriptorAt(m_srvHeap.GetIndex(m_lightsSRV)).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
				}

				uploadBuffer.Initialize(m_pDevice,
					{
						.debugName{ L"LightUploadBuffer" },
						.usage{ EBufferUsage::Constant },
						.memory{ EBufferMemory::CPU },
						.byteSize{ lightByteSize  },
						.initialState{ EResourceState::Undefined }
					});
				void* pData{ nullptr };
				uploadBuffer.Map(0, lightByteSize, &pData);
				dU32 count{ 0 };

				Viewport viewport{ 0.0, 0.0, SHADOW_MAP_RESOLUTION_F, SHADOW_MAP_RESOLUTION_F, 0.0f, 1.0f };
				Scissor scissor{ 0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
				frame.commandList.SetViewports(1, &viewport);
				frame.commandList.SetScissors(1, &scissor);

				dU32 cubeShadowIndex{ 0 };
				dU32 shadowIndex{ 0 };
				dU32 matrixIndex{ 0 };
				view.each([&](const Dune::Light& sceneLight)
					{
						Light light{};
						light.color = sceneLight.color;
						light.intensity = sceneLight.intensity;
						switch (sceneLight.type)
						{
						case ELightType::Directional:
							DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
							break;
						case ELightType::Point:
							light.range = sceneLight.range;
							light.position = sceneLight.position;
							light.flags |= fIsPoint;
							break;
						case ELightType::Spot:
							light.range = sceneLight.range;
							light.position = sceneLight.position;
							DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
							light.angle = DirectX::XMScalarCos(DirectX::XMConvertToRadians(sceneLight.angle));
							light.penumbra = 1.0f / (DirectX::XMScalarCos(DirectX::XMConvertToRadians(sceneLight.angle * (1.0f - sceneLight.penumbra))) - light.angle);
							light.flags |= fIsSpot;
							break;
						}

						if (sceneLight.castShadow)
						{
							light.flags |= fCastShadow;
							Descriptor dsv = m_dsvHeap.Allocate();
							Descriptor srv = frame.srvHeap.Allocate(1);

							if (sceneLight.type == ELightType::Point)
							{
								if (m_cubeShadowMaps.size() <= cubeShadowIndex)
									m_cubeShadowMaps.emplace_back();
								if ( m_lightMatrices.size() <= matrixIndex)
									m_lightMatrices.emplace_back();
								Texture& shadowMap = m_cubeShadowMaps[cubeShadowIndex++];
								dMatrix4x4& lightMatrix = m_lightMatrices[matrixIndex];
								light.matrixIndex = matrixIndex++;
								if (!shadowMap.Get())
								{
									shadowMap.Initialize(m_pDevice,
										{
											.debugName = L"DepthBuffer",
											.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
											.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 6},
											.format = EFormat::D32_FLOAT,
											.clearValue = {1.f, 1.f, 1.f, 1.f},
											.initialState = EResourceState::DepthStencil
										});
								}
								else
								{
									m_barrier.PushTransition(shadowMap.Get(), EResourceState::ShaderResource, EResourceState::DepthStencil);
									frame.commandList.Transition(m_barrier);
									m_barrier.Reset();
								}

								m_pDevice->CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT, .dimension = ESRVDimension::TextureCube });
								light.shadowIndex = frame.srvHeap.GetIndex(srv);

								dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.f), 1.0f, 0.1f, sceneLight.range) };
								dVec eye{ sceneLight.position.x, sceneLight.position.y, sceneLight.position.z };
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
									m_pDevice->CreateDSV(dsv, shadowMap, { .firstArraySlice = faceIndex, .arraySize = 1, .dimension = EDSVDimension::Texture2DArray });
									frame.commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
									frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
									DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrices[faceIndex] * projectionMatrix);
									m_shadowPass.Render(scene, frame.commandList, lightMatrix);
								}
								m_barrier.PushTransition(shadowMap.Get(), EResourceState::DepthStencil, EResourceState::ShaderResource);
								frame.commandList.Transition(m_barrier);
								m_barrier.Reset();
								DirectX::XMStoreFloat4x4(&lightMatrix, projectionMatrix);
							} 
							else 
							{
								if (m_shadowMaps.size() <= shadowIndex)
									m_shadowMaps.emplace_back();
								if ( m_lightMatrices.size() <= matrixIndex)
									m_lightMatrices.emplace_back();
								Texture& shadowMap = m_shadowMaps[shadowIndex++];
								dMatrix4x4& lightMatrix = m_lightMatrices[matrixIndex];
								light.matrixIndex = matrixIndex++;
								if (!shadowMap.Get())
								{
									shadowMap.Initialize(m_pDevice,
										{
											.debugName = L"DepthBuffer",
											.usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource,
											.dimensions = {SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1},
											.format = EFormat::D32_FLOAT,
											.clearValue = {1.f, 1.f, 1.f, 1.f},
											.initialState = EResourceState::DepthStencil
										});
								}
								else 
								{
									m_barrier.PushTransition(shadowMap.Get(), EResourceState::ShaderResource, EResourceState::DepthStencil);
									frame.commandList.Transition(m_barrier);
									m_barrier.Reset();
								}

								m_pDevice->CreateSRV(srv, shadowMap, { .format = EFormat::R32_FLOAT });
								m_pDevice->CreateDSV(dsv, shadowMap, {});
								frame.commandList.ClearDepthBuffer(dsv, 1.0f, 0.0f);
								frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);

								dVec up{ 0.f, 1.f, 0.f, 0.f };
								dVec to{ DirectX::XMLoadFloat3(&light.direction) };
								dVec axis = DirectX::XMVector3Cross(up, to);
								if (DirectX::XMVector3Equal(axis, { 0.0f, 0.0f, 0.0f }))
									up = { 0.f, 0.f, 1.f, 0.f };

								switch (sceneLight.type)
								{
								case ELightType::Directional:
								{
									float shadowWidth{ 4500.f }; // Hardcoded for sponza
									dVec eye{ 0.f, 0.f, 0.f };
									dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
									dMatrix projectionMatrix{ DirectX::XMMatrixOrthographicLH(shadowWidth, shadowWidth, -shadowWidth, shadowWidth) };
									DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
									break;
								}
								case ELightType::Spot:
									dVec eye{ sceneLight.position.x, sceneLight.position.y, sceneLight.position.z };
									dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
									dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(sceneLight.angle * 2.0f), 1.0f, 0.1f, sceneLight.range) };
									DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
									break;
								}

								m_shadowPass.Render(scene, frame.commandList, lightMatrix);
								light.shadowIndex = frame.srvHeap.GetIndex(srv);
								m_barrier.PushTransition(shadowMap.Get(), EResourceState::DepthStencil, EResourceState::ShaderResource);
								frame.commandList.Transition(m_barrier);
								m_barrier.Reset();
							}
							m_dsvHeap.Free(dsv);
						}
						memcpy((Light*)pData + count++, &light, sizeof(Light));
					}
				);
				uploadBuffer.Unmap(0, lightByteSize);
				frame.commandList.CopyBufferRegion(m_lightBuffer, 0, uploadBuffer, 0, lightByteSize);
				frame.buffersToRelease.push(uploadBuffer);

				if (matrixIndex > 0)
				{
					dU32 matricesByteSize = (dU32)((matrixIndex) * sizeof(dMatrix4x4));
					if (m_lightMatricesBuffer.GetByteSize() < matricesByteSize)
					{
						if (m_lightMatricesBuffer.Get())
							frame.buffersToRelease.push(m_lightMatricesBuffer);
						m_lightMatricesBuffer.Initialize(m_pDevice,
							{
								.debugName{ L"LightMatricesBuffer" },
								.usage{ EBufferUsage::Constant },
								.memory{ EBufferMemory::GPU },
								.byteSize{ matricesByteSize  },
								.byteStride { sizeof(Light) },
								.initialState{ EResourceState::Undefined }
							});
						m_pDevice->CreateSRV(m_lightMatricesSRV, m_lightMatricesBuffer);
						m_pDevice->CopyDescriptors(1, m_lightMatricesSRV.cpuAddress, frame.srvHeap.GetDescriptorAt(m_srvHeap.GetIndex(m_lightMatricesSRV)).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
					}

					Buffer matricesUploadBuffer{};
					matricesUploadBuffer.Initialize(m_pDevice,
						{
							.debugName{ L"MatricesUploadBuffer" },
							.usage{ EBufferUsage::Constant },
							.memory{ EBufferMemory::CPU },
							.byteSize{ matricesByteSize  },
							.initialState{ EResourceState::Undefined }
						});
					void* pMatricesData{ nullptr };
					matricesUploadBuffer.Map(0, matricesByteSize, &pMatricesData);
					memcpy(pMatricesData, m_lightMatrices.data(), matricesByteSize);
					matricesUploadBuffer.Unmap(0, matricesByteSize);
					frame.commandList.CopyBufferRegion(m_lightMatricesBuffer, 0, matricesUploadBuffer, 0, matricesByteSize);
					frame.buffersToRelease.push(matricesUploadBuffer);
				}
			}
			globals.lightCount = lightCount;
			globals.lightBufferIndex = m_srvHeap.GetIndex(m_lightsSRV);
			globals.lightMatricesIndex = m_srvHeap.GetIndex(m_lightMatricesSRV);
		}

		m_barrier.PushTransition(m_swapchain.GetBackBuffer(m_frameIndex).Get(), EResourceState::Present, EResourceState::RenderTarget);
		m_barrier.PushTransition(frame.colorTarget.Get(), EResourceState::ShaderResource, EResourceState::RenderTarget);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();

		Descriptor dsv = m_depthBufferDSV;
		frame.commandList.ClearRenderTargetView(frame.colorTargetRTV, frame.colorTarget.GetClearValue());
		frame.commandList.ClearDepthBuffer(dsv, m_depthBuffer.GetClearValue()[0], 0);

		Viewport viewport{ 0.0, 0.0, (float)m_pWindow->GetWidth(), (float)m_pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, m_pWindow->GetWidth(), m_pWindow->GetHeight() };
		frame.commandList.SetViewports(1, &viewport);
		frame.commandList.SetScissors(1, &scissor);
		
		frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
		m_depthPrepass.Render(scene, frame.commandList, globals.viewProjectionMatrix);

		frame.commandList.SetRenderTarget(&frame.colorTargetRTV.cpuAddress, 1, &dsv.cpuAddress);
		m_forwardPass.Render(scene, frame.srvHeap, frame.commandList, globals);

		Descriptor source = frame.srvHeap.Allocate(1);

		m_barrier.PushTransition(frame.colorTarget.Get(), EResourceState::RenderTarget, EResourceState::ShaderResource);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();
		frame.commandList.SetRenderTarget(&frame.backBufferRTV.cpuAddress, 1, nullptr);
		Descriptor colorTargetSRV = frame.srvHeap.GetDescriptorAt(m_srvHeap.GetIndex(frame.colorTargetSRV));
		m_tonemappingPass.Render(frame.commandList, colorTargetSRV);

		if (m_pImGui) 
		{
			frame.commandList.SetDescriptorHeaps(m_srvImGuiHeap);
			m_pImGui->Render(frame.commandList);
		}

		m_barrier.PushTransition(m_swapchain.GetBackBuffer(m_frameIndex).Get(), EResourceState::RenderTarget, EResourceState::Present);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();

		frame.commandList.Close();
		m_commandQueue.ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_commandQueue.Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % 3;
	}
}
