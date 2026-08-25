#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/ImGUIWrapper.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Scene/Camera.h"
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx12.h>

namespace Dune::Graphics
{
	void Renderer::Initialize(RenderContext& context, Window& window)
	{
		m_pRenderContext = &context;
		m_pWindow = &window;

		Device& device = m_pRenderContext->GetDevice();

		dU32 width = m_pWindow->GetWidth();
		dU32 height = m_pWindow->GetHeight();
		m_depthBuffer.Initialize(device,
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
			frame.commandAllocator.Initialize(device, ECommandType::Direct);
			frame.commandList.Initialize(device, ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
			frame.hdrTarget.Initialize(device, colorTargetDesc);
			frame.srvHeap.Initialize(device, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = kPersistentSRVCapacity + kTransientSRVCapacity, .isShaderVisible = true });
			frame.samplerHeap.Initialize(device, { .type = EDescriptorHeapType::Sampler, .capacity = 64, .isShaderVisible = true });
		}

		m_barrier.Initialize(16);
		m_fence.Initialize(device, 0);
		m_commandQueue.Initialize(device, ECommandType::Direct);
		m_swapchain.Initialize(device, m_pWindow, &m_commandQueue, { .latency = kFramesInFlight });

		DescriptorHeapDesc heapDesc { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = 64, .isShaderVisible = true };
		m_srvImGuiHeap.Initialize(device, heapDesc);

		heapDesc.capacity = kPersistentSRVCapacity;
		heapDesc.isShaderVisible = false;
		m_srvHeap.Initialize(device, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = EDescriptorHeapType::RTV;
		m_rtvHeap.Initialize(device, heapDesc);

		heapDesc.type = EDescriptorHeapType::DSV;
		m_dsvHeap.Initialize(device, heapDesc);

		for (dU32 i = 0; i < kFramesInFlight; i++)
		{
			Frame& frame = m_frames[i];
			frame.backBufferRTV = m_rtvHeap.Allocate();
			frame.hdrTargetRTV = m_rtvHeap.Allocate();
			frame.hdrTargetSRV = m_srvHeap.Allocate();
			device.CreateRTV(frame.backBufferRTV, m_swapchain.GetBackBuffer(i), {});
			device.CreateRTV(frame.hdrTargetRTV, frame.hdrTarget, {});
			device.CreateSRV(frame.hdrTargetSRV, frame.hdrTarget);
		}

		m_depthBufferDSV = m_dsvHeap.Allocate();
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		m_forwardPass.Initialize(device);
		m_depthPrepass.Initialize(device);
		m_tonemappingPass.Initialize(*this);

		m_lightsSRV = m_srvHeap.Allocate();
		m_lightMatrices.srv = m_srvHeap.Allocate();

		RegisterRenderPass<Shadow>();
		RegisterSharedResource(EResourceTag::LightMatrices, &m_lightMatrices);
		RegisterSharedResource(EResourceTag::ShadowMaps, &m_shadowMaps);
	}

	void Renderer::Destroy()
	{
		for (Frame& frame : m_frames)
		{
			while (!frame.buffersToRelease.empty())
			{
				frame.buffersToRelease.front().Destroy();
				frame.buffersToRelease.pop();
			}
			WaitForFrame(frame);
			m_rtvHeap.Free(frame.backBufferRTV);
			m_rtvHeap.Free(frame.hdrTargetRTV);
			m_srvHeap.Free(frame.hdrTargetSRV);
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
			frame.hdrTarget.Destroy();
			frame.srvHeap.Destroy();
			frame.samplerHeap.Destroy();
		}
		m_dsvHeap.Free(m_depthBufferDSV);
		m_srvHeap.Free(m_lightsSRV);
		m_srvHeap.Free(m_lightMatrices.srv);

		for (Texture& shadow : m_shadowMaps.shadows)
			shadow.Destroy();
		for (Texture& shadow : m_shadowMaps.cubeShadows)
			shadow.Destroy();

		m_forwardPass.Destroy();
		m_depthPrepass.Destroy();
		m_tonemappingPass.Destroy();

		for (RenderPass& pass : m_passes)
			pass.pShutdown(*this, pass.pData);

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

		if (m_lightMatrices.buffer.Get())
			m_lightMatrices.buffer.Destroy();
	}

	void FillLight(const Dune::Light& sceneLight, Light& light)
	{
		light.color = sceneLight.color;
		switch (sceneLight.type)
		{
		case ELightType::Directional:
			light.intensity = sceneLight.intensity;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			break;
		case ELightType::Point:
		{
			float lightSolidAngle = 4.0f * DirectX::XM_PI;
			float candelaIntensity = sceneLight.intensity / lightSolidAngle;
			light.intensity = candelaIntensity / (0.01f * 0.01f);
		}
		light.range = sceneLight.range;
		light.position = sceneLight.position;
		light.flags |= fIsPoint;
		break;
		case ELightType::Spot:
			light.range = sceneLight.range;
			light.position = sceneLight.position;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			light.angle = DirectX::XMScalarCos(sceneLight.angle);
			{
				float lightSolidAngle = 2.0f * DirectX::XM_PI * (1.0f - light.angle);
				float candelaIntensity = sceneLight.intensity / lightSolidAngle;
				light.intensity = candelaIntensity / (0.01f * 0.01f);
			}
			light.penumbra = 1.0f / (DirectX::XMScalarCos(sceneLight.angle * (1.0f - sceneLight.penumbra)) - light.angle);
			light.flags |= fIsSpot;
			break;
		}
		if ( sceneLight.castShadow )
			light.flags |= fCastShadow;
	}

	void Renderer::GatherFrameData(Scene& scene)
	{
		m_frameData.lights.allActive.clear();
		m_frameData.lights.shadowCasters.clear();
		m_frameData.instances.clear();

		scene.registry.view<const Dune::Light>().each([&](const Dune::Light& sceneLight)
		{
			if (sceneLight.intensity <= 0.0f)
				return;
			Light light{};
			FillLight(sceneLight, light);
			m_frameData.lights.allActive.push_back(light);
			if (sceneLight.castShadow)
				m_frameData.lights.shadowCasters.push_back((dU32)m_frameData.lights.allActive.size()-1);
		});

		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
		{
			RenderInstance instance;
			DirectX::XMStoreFloat4x4(&instance.objectToWorld,
				DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
				DirectX::XMMatrixRotationQuaternion(transform.rotation) *
				DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
			);
			instance.data = renderData;
			m_frameData.instances.emplace_back(instance);
		});
	}

	void Renderer::OnResize(dU32 width, dU32 height)
	{
		TextureDesc hdrTargetDesc
		{
			.debugName = L"HDRTarget",
			.usage{ ETextureUsage::RenderTarget | ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ EFormat::R16G16B16A16_FLOAT },
			.initialState{ EResourceState::ShaderResource },
		};

		Device& device = m_pRenderContext->GetDevice();
		for (Frame& f : m_frames)
		{
			WaitForFrame(f);
			f.hdrTarget.Destroy();
			f.hdrTarget.Initialize(device, hdrTargetDesc);
			device.CreateSRV(f.hdrTargetSRV, f.hdrTarget);
			device.CreateRTV(f.hdrTargetRTV, f.hdrTarget, {});
		}

		m_swapchain.Resize(width, height);
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < kFramesInFlight; i++)
			device.CreateRTV(m_frames[i].backBufferRTV, m_swapchain.GetBackBuffer(i), {});

		m_depthBuffer.Destroy();
		m_depthBuffer.Initialize(device,
			{
				.debugName = L"DepthBuffer",
				.usage = ETextureUsage::DepthStencil,
				.dimensions = { width, height, 1 },
				.format = EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = EResourceState::DepthStencil
			});
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
	}

	void Renderer::WaitForFrame(const Frame& frame)
	{
		const dU64 fenceValue{ frame.fenceValue };
		if ( m_fence.GetValue() < fenceValue )
			m_fence.Wait(fenceValue);
	}

	void Renderer::Render(Scene& scene, Camera& camera)
	{
		GatherFrameData(scene);

		Device& device = m_pRenderContext->GetDevice();
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

		RenderPassContext context
		{
			.pRenderer = this,
			.pFrame = &frame,
			.pFrameData = &m_frameData,
			.pCommandList = &frame.commandList,
			.pBarrier = &m_barrier,
		};

		device.CopyDescriptors(m_srvHeap.GetCapacity(), m_srvHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress(), EDescriptorHeapType::SRV_CBV_UAV);
		frame.srvHeap.Allocate(kPersistentSRVCapacity);

		// Render shadow, hardcoded for now since I didn't port other render pass
		RenderPass& renderPass = m_passes[0];
		renderPass.pExecute(context, renderPass.pData);

		dVector<Light>& allLights = m_frameData.lights.allActive;
		dU32 lightCount = (dU32)allLights.size();
		if (lightCount > 0)
		{
			Buffer uploadBuffer{};
			dU32 lightByteSize = (dU32)((lightCount) * sizeof(Light));
			uploadBuffer.Initialize(device,
				{
					.debugName{ L"LightUploadBuffer" },
					.memory{ EBufferMemory::CPU },
					.byteSize{ lightByteSize },
					.initialState{ EResourceState::Undefined }
				});

			void* pData{ nullptr };
			uploadBuffer.Map(0, lightByteSize, &pData);
			memcpy(pData, allLights.data(), sizeof(Light) * allLights.size());

			if (m_lightBuffer.GetByteSize() < lightByteSize)
			{
				if (m_lightBuffer.Get())
					frame.buffersToRelease.push(m_lightBuffer);
				m_lightBuffer.Initialize(device,
					{
						.debugName{ L"LightBuffer" },
						.memory{ EBufferMemory::GPU },
						.byteSize{ lightByteSize  },
						.initialState{ EResourceState::Undefined }
					});
				device.CreateSRV(m_lightsSRV, m_lightBuffer, { .elementCount = lightCount, .byteStride = sizeof(Light) });
				device.CopyDescriptors(1, m_lightsSRV.cpuAddress, frame.srvHeap.GetDescriptorAt(m_srvHeap.GetIndex(m_lightsSRV)).cpuAddress, EDescriptorHeapType::SRV_CBV_UAV);
			}
			uploadBuffer.Unmap(0, lightByteSize);
			frame.commandList.CopyBufferRegion(m_lightBuffer, 0, uploadBuffer, 0, lightByteSize);
			frame.buffersToRelease.push(uploadBuffer);
		}

		ForwardGlobals globals;
		ComputeViewProjectionMatrix(camera, nullptr, nullptr, &globals.viewProjectionMatrix);
		globals.cameraPosition = camera.position;
		globals.lightCount = lightCount;
		globals.lightBufferIndex = m_srvHeap.GetIndex(m_lightsSRV);
		globals.lightMatricesIndex = m_srvHeap.GetIndex(m_lightMatrices.srv);

		m_barrier.PushTransition(m_swapchain.GetBackBuffer(m_frameIndex).Get(), EResourceState::Present, EResourceState::RenderTarget);
		m_barrier.PushTransition(frame.hdrTarget.Get(), EResourceState::ShaderResource, EResourceState::RenderTarget);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();

		Descriptor dsv = m_depthBufferDSV;
		frame.commandList.ClearRenderTargetView(frame.hdrTargetRTV, frame.hdrTarget.GetClearValue());
		frame.commandList.ClearDepthBuffer(dsv, m_depthBuffer.GetClearValue()[0], 0);

		Viewport viewport{ 0.0, 0.0, (float)m_pWindow->GetWidth(), (float)m_pWindow->GetHeight(), 0.0f, 1.0f };
		Scissor scissor{ 0, 0, m_pWindow->GetWidth(), m_pWindow->GetHeight() };
		frame.commandList.SetViewports(1, &viewport);
		frame.commandList.SetScissors(1, &scissor);

		frame.commandList.SetRenderTarget(nullptr, 0, &dsv.cpuAddress);
		m_depthPrepass.Render(m_frameData, m_pRenderContext->GetResourceManager(), frame.commandList, globals.viewProjectionMatrix);

		frame.commandList.SetRenderTarget(&frame.hdrTargetRTV.cpuAddress, 1, &dsv.cpuAddress);
		m_forwardPass.Render(m_frameData, *this, frame.commandList, globals);

		m_barrier.PushTransition(frame.hdrTarget.Get(), EResourceState::RenderTarget, EResourceState::ShaderResource);
		frame.commandList.Transition(m_barrier);
		m_barrier.Reset();
		frame.commandList.SetRenderTarget(&frame.backBufferRTV.cpuAddress, 1, nullptr);
		Descriptor hdrTargetSRV = frame.srvHeap.GetDescriptorAt(m_srvHeap.GetIndex(frame.hdrTargetSRV));
		m_tonemappingPass.Render(*this, frame.commandList, hdrTargetSRV);

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
		m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
	}
	
	void Renderer::RegisterSharedResource(EResourceTag id, void* pResource)
	{
		dU32 index = (dU32)id;
		if (index >= m_sharedResources.size() )
			m_sharedResources.resize(index+1);
		m_sharedResources[index] = pResource;
	}
}
