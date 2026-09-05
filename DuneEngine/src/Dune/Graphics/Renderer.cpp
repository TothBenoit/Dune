#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/ImGUIWrapper.h"
#include "Dune/Graphics/RenderPass/ClearDepth.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
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
			frame.srvHeap.Initialize(device, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = ResourceManager::kSharedSRVCapacity + kPersistentSRVCapacity + kTransientSRVCapacity, .isShaderVisible = true });
			frame.samplerHeap.Initialize(device, { .type = EDescriptorHeapType::Sampler, .capacity = 64, .isShaderVisible = true });
			frame.materialBuffer.Initialize(device, { .debugName = L"MaterialBuffer", .memory = EBufferMemory::GPU, .byteSize = sizeof(MaterialData)});
		}

		m_barrier.Initialize(kBarrierCapacity);
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

			frame.hdrTargetHandle = RegisterTexture(&frame.hdrTarget, EResourceState::ShaderResource);
			frame.backBufferHandle = RegisterTexture(&m_swapchain.GetBackBuffer(i), EResourceState::Present);
			
			frame.materialBufferSRV = m_srvHeap.Allocate();
			device.CreateSRV(frame.materialBufferSRV, frame.materialBuffer, { .elementCount = 1, .byteStride = sizeof(MaterialData) });
		}

		m_depthBufferDSV = m_dsvHeap.Allocate();
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
		m_depthBufferHandle = RegisterTexture(&m_depthBuffer, EResourceState::DepthStencil);

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		RegisterRenderPass<ClearDepth>();
		RegisterRenderPass<DepthPrepass>();
		RegisterRenderPass<Shadow>();
		RegisterRenderPass<LightUpload>();
		RegisterRenderPass<Forward>();
		RegisterRenderPass<Tonemapping>();
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
			m_srvHeap.Free(frame.materialBufferSRV);
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
			frame.hdrTarget.Destroy();
			frame.materialBuffer.Destroy();
			frame.srvHeap.Destroy();
			frame.samplerHeap.Destroy();
		}
		m_dsvHeap.Free(m_depthBufferDSV);

		for (RenderPass& pass : m_passes)
			pass.pShutdown(*this, pass.pData);
		m_passes.clear();

		for (ResourceEntry& entry : m_resources)
		{
			if (entry.pPhysicalResource && !entry.isExternal)
			{
				switch (entry.type)
				{
				case EResourceType::Texture: 
					static_cast<Texture*>(entry.pPhysicalResource)->Destroy();
					delete entry.pPhysicalResource;
					break;
				case EResourceType::Buffer:
					static_cast<Buffer*>(entry.pPhysicalResource)->Destroy();
					delete entry.pPhysicalResource;
					break;
				}
			}
		}
		m_resources.clear();

		m_srvHeap.Destroy();
		m_srvImGuiHeap.Destroy();
		m_rtvHeap.Destroy();
		m_dsvHeap.Destroy();
		m_barrier.Destroy();
		m_depthBuffer.Destroy();
		m_commandQueue.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();
	}

	ResourceHandle Renderer::CreateTexture(const TextureDesc& desc)
	{
		Texture* pTexture = new Texture();
		pTexture->Initialize(m_pRenderContext->GetDevice(), desc);

		const dU32 subresourceCount = desc.dimensions[2] * desc.mipLevels;
		ResourceHandle handle = RegisterTexture(pTexture, desc.initialState, subresourceCount);
		m_resources[handle].isExternal = false;
		return handle;
	}

	ResourceHandle Renderer::CreateBuffer(const BufferDesc& desc)
	{
		Buffer* pBuffer = new Buffer();
		pBuffer->Initialize(m_pRenderContext->GetDevice(), desc);

		ResourceHandle handle = RegisterBuffer(pBuffer, desc.initialState);
		m_resources[handle].isExternal = false;
		return handle;
	}

	ResourceHandle Renderer::RegisterTexture(Texture* pTexture, EResourceState initialState, dU32 subresourceCount)
	{
		Assert(subresourceCount > 0);
		ResourceHandle handle = (ResourceHandle)m_resources.size();
		ResourceEntry entry{};
		entry.pPhysicalResource = pTexture;
		entry.subresourceStates.assign(subresourceCount, initialState);
		entry.isExternal = true;
		entry.type = EResourceType::Texture;
		m_resources.push_back(std::move(entry));
		return handle;
	}

	ResourceHandle Renderer::RegisterBuffer(Buffer* pBuffer, EResourceState initialState)
	{
		ResourceHandle handle = (ResourceHandle)m_resources.size();
		ResourceEntry entry{};
		entry.pPhysicalResource = pBuffer;
		entry.resourceState = initialState;
		entry.isExternal = true;
		entry.type = EResourceType::Buffer;
		m_resources.push_back(std::move(entry));
		return handle;
	}

	void Renderer::SetPhysicalResource(ResourceHandle handle, Resource* pResource, EResourceState state)
	{
		Assert(handle < m_resources.size());
		ResourceEntry& entry = m_resources[handle];
		entry.pPhysicalResource = pResource;
		switch (entry.type)
		{
		case EResourceType::Texture:
			for (EResourceState& subresourceState : entry.subresourceStates)
				subresourceState = state;
			break;
		case EResourceType::Buffer:
			entry.resourceState = state;
			break;
		}
	}

	Texture& Renderer::GetTexture(ResourceHandle handle)
	{
		Assert(handle < m_resources.size());
		Assert(m_resources[handle].type == EResourceType::Texture);
		return *static_cast<Texture*>(m_resources[handle].pPhysicalResource);
	}

	Buffer& Renderer::GetBuffer(ResourceHandle handle)
	{
		Assert(handle < m_resources.size());
		Assert(m_resources[handle].type == EResourceType::Buffer);
		return *static_cast<Buffer*>(m_resources[handle].pPhysicalResource);
	}

	void Renderer::TransitionResource(const ResourceAccess& access)
	{
		ResourceEntry& entry = m_resources[access.handle];
		switch (entry.type)
		{
		case EResourceType::Texture:
		{
			if (access.subresource != kAllSubresources)
			{
				Assert(access.subresource < entry.subresourceStates.size());
				EResourceState& state = entry.subresourceStates[access.subresource];
				if (state == access.state)
					return;
				m_barrier.PushTransition(*entry.pPhysicalResource, state, access.state, access.subresource);
				state = access.state;
				return;
			}

			bool uniform = true;
			const EResourceState first = entry.subresourceStates[0];
			for (EResourceState state : entry.subresourceStates)
			{
				if (state != first)
				{
					uniform = false;
					break;
				}
			}

			if (uniform)
			{
				if (first == access.state)
					return;
				m_barrier.PushTransition(*entry.pPhysicalResource, first, access.state, kAllSubresources);
				for (EResourceState& state : entry.subresourceStates)
					state = access.state;
			}
			else
			{
				for (dU32 i = 0; i < (dU32)entry.subresourceStates.size(); i++)
				{
					EResourceState& state = entry.subresourceStates[i];
					if (state == access.state)
						continue;
					m_barrier.PushTransition(*entry.pPhysicalResource, state, access.state, i);
					state = access.state;
				}
			}
			break;
		}
		case EResourceType::Buffer:
			Assert(access.subresource == kAllSubresources);
			if (entry.resourceState == access.state)
				return;
			m_barrier.PushTransition(*entry.pPhysicalResource, entry.resourceState, access.state, kAllSubresources);
			entry.resourceState = access.state;
			break;
		}
	}

	void Renderer::FlushBarriers(CommandList& commandList)
	{
		if (m_barrier.GetBarrierCount() != 0)
		{
			commandList.Transition(m_barrier);
			m_barrier.Reset();
		}
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
		m_frameData.drawItems.clear();

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

		ResourceManager& resourceManager = m_pRenderContext->GetResourceManager();
		m_frameData.blendingMaterialCount = 0;
		scene.registry.view<const Transform, const RenderData>().each([&](const Transform& transform, const RenderData& renderData)
		{
			dMatrix4x4 objectToWorld;
			DirectX::XMStoreFloat4x4(&objectToWorld,
				DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
				DirectX::XMMatrixRotationQuaternion(transform.rotation) *
				DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
			);

			Mesh& mesh = resourceManager.GetMesh(renderData.meshIdx);
			Assert(renderData.materialSlotCount == mesh.GetMaterialSlotCount());
			for (const SubMesh& subMesh : mesh.GetSubMeshes())
			{
				DrawItem& drawItem = m_frameData.drawItems.emplace_back();
				drawItem.objectToWorld = objectToWorld;
				drawItem.meshIdx = renderData.meshIdx;
				drawItem.materialIdx = resourceManager.GetMaterialID(renderData.materialSlotStart + subMesh.materialSlot);
				drawItem.indexOffset = subMesh.indexOffset;
				drawItem.indexCount = subMesh.indexCount;
				drawItem.vertexOffset = subMesh.vertexOffset;
				const Material& material = resourceManager.GetMaterial(drawItem.materialIdx);
				drawItem.materialVariant = material.GetVariant();
				m_frameData.blendingMaterialCount += material.alphaMode == EAlphaMode::Blend ? 1 : 0;
			}
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
			SetPhysicalResource(f.hdrTargetHandle, &f.hdrTarget, EResourceState::ShaderResource);
		}

		m_swapchain.Resize(width, height);
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < kFramesInFlight; i++)
		{
			device.CreateRTV(m_frames[i].backBufferRTV, m_swapchain.GetBackBuffer(i), {});
			SetPhysicalResource(m_frames[i].backBufferHandle, &m_swapchain.GetBackBuffer(i), EResourceState::Present);
		}

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
		SetPhysicalResource(m_depthBufferHandle, &m_depthBuffer, EResourceState::DepthStencil);
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

		std::sort(m_frameData.drawItems.begin(), m_frameData.drawItems.end(),
			[](const DrawItem& a, const DrawItem& b)
			{
				return a.materialVariant < b.materialVariant;
			}
		);

		dU32 blendingMaterialStart = (dU32)m_frameData.drawItems.size() - m_frameData.blendingMaterialCount;
		const dVec3& eye = camera.position;
		std::sort(m_frameData.drawItems.begin() + blendingMaterialStart, m_frameData.drawItems.end(),
			[&eye](const DrawItem& a, const DrawItem& b)
			{
				const dMatrix4x4& ma = a.objectToWorld;
				const float adx = ma._41 - eye.x;
				const float ady = ma._42 - eye.y;
				const float adz = ma._43 - eye.z;
				const float aToCamDistSq = adx * adx + ady * ady + adz * adz;

				const dMatrix4x4& mb = b.objectToWorld;
				const float bdx = mb._41 - eye.x;
				const float bdy = mb._42 - eye.y;
				const float bdz = mb._43 - eye.z;
				const float bToCamDistSq = bdx * bdx + bdy * bdy + bdz * bdz;

				return aToCamDistSq > bToCamDistSq;
			}
		);

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
		
		ResourceManager& resourceManager = m_pRenderContext->GetResourceManager();
		const dVector<Material>& materials = resourceManager.GetMaterials();
		dU32 materialCount = (dU32)materials.size();
		dU32 materialsByteSize = sizeof(MaterialData) * materialCount;
		if (frame.materialBuffer.GetByteSize() < materialsByteSize)
		{
			frame.materialBuffer.Destroy();
			frame.materialBuffer.Initialize(device, { .debugName = L"MaterialBuffer", .memory = EBufferMemory::GPU, .byteSize = materialsByteSize });
			device.CreateSRV(frame.materialBufferSRV, frame.materialBuffer, { .elementCount = materialCount, .byteStride = sizeof(MaterialData) });
		}

		const BlockDescriptorHeap& sharedHeap = resourceManager.GetSRVHeap();
		Assert(frame.srvHeap.GetCapacity() >= m_srvHeap.GetCapacity() + sharedHeap.GetCapacity() + kTransientSRVCapacity);

		dU32 sharedSRVCapacity = m_frameData.reservedSharedSRV = sharedHeap.GetCapacity();
		device.CopyDescriptors(sharedSRVCapacity, sharedHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress(), EDescriptorHeapType::SRV_CBV_UAV);

		device.CopyDescriptors(m_srvHeap.GetCapacity(), m_srvHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress() + m_frameData.reservedSharedSRV * frame.srvHeap.GetDescriptorSize(), EDescriptorHeapType::SRV_CBV_UAV);
		frame.srvHeap.Allocate(sharedSRVCapacity + m_srvHeap.GetCapacity());

		// TODO : Add MaterialUpload render pass
		{
			Buffer uploadBuffer;
			uploadBuffer.Initialize(device, { .debugName = L"UploadMaterialBuffer", .memory = EBufferMemory::CPU, .byteSize = materialsByteSize });
			MaterialData* pData{ nullptr };
			uploadBuffer.Map(0, materialsByteSize, (void**)&pData);
			for (dU32 materialIdx = 0; materialIdx < materialCount; materialIdx++)
			{
				const Material& material = materials[materialIdx];
				memcpy(pData + materialIdx, &material.shaderData, sizeof(MaterialData));
			}
			uploadBuffer.Unmap(0, materialsByteSize);
			frame.commandList.CopyBufferRegion(frame.materialBuffer, 0, uploadBuffer, 0, materialsByteSize);
			frame.buffersToRelease.push(uploadBuffer);
			m_barrier.PushTransition(frame.materialBuffer, EResourceState::CopyDest, EResourceState::ShaderResource);
		}

		RenderPassContext context
		{
			.pRenderer = this,
			.pCamera = &camera,
			.pFrameData = &m_frameData,
			.pBarrier = &m_barrier,
		};

		for (RenderPass& pass : m_passes)
		{
			pass.builder.Reset();
			pass.pSetup(pass.builder, context, pass.pData);
		}

		for (RenderPass& pass : m_passes)
		{
			if (pass.builder.IsEmpty())
				continue;

			for (const ResourceAccess& access : pass.builder.GetReads())
				TransitionResource(access);
			for (const ResourceAccess& access : pass.builder.GetWrites())
				TransitionResource(access);
			FlushBarriers(frame.commandList);

			pass.pExecute(context, pass.pData);
		}

		if (m_pImGui)
		{
			frame.commandList.SetDescriptorHeaps(m_srvImGuiHeap);
			m_pImGui->Render(frame.commandList);
		}

		TransitionResource({ .handle = frame.backBufferHandle, .state = EResourceState::Present });
		FlushBarriers(frame.commandList);

		frame.commandList.Close();
		m_commandQueue.ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_commandQueue.Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
	}
}
