#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include <Dune/Graphics/FrameData.h>
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/ImGUIWrapper.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/PSOCache.h"
#include "Dune/Component/Camera.h"

namespace Dune::Graphics
{
	void Renderer::Initialize(Device& device, CommandQueue& commandQueue, PSOCache& psoCache, Window& window)
	{
		m_pDevice = &device;
		m_pCommandQueue = &commandQueue;
		m_pPSOCache = &psoCache;
		m_pWindow = &window;

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

		TextureDesc hdrTargetDesc
		{
			.debugName = L"HDRTarget",
			.usage{ ETextureUsage::RenderTarget | ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ EFormat::R16G16B16A16_FLOAT },
			.initialState{ EResourceState::ShaderResource },
		};
		m_hdrTarget.Initialize(device, hdrTargetDesc);

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(device, ECommandType::Direct);
			frame.commandList.Initialize(device, ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
			frame.srvHeap.Initialize(device, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = ResourceManager::kSharedSRVCapacity + kPersistentSRVCapacity + kTransientSRVCapacity, .isShaderVisible = true });
			frame.samplerHeap.Initialize(device, { .type = EDescriptorHeapType::Sampler, .capacity = 64, .isShaderVisible = true });
			frame.uploadBuffer.Initialize(device, { .debugName = L"UploadBuffer", .memory = EBufferMemory::CPU, .byteSize = kUploadBufferByteSize });
			void* pUploadAddress{ nullptr };
			frame.uploadBuffer.Map(0, 0, &pUploadAddress);
			frame.staging.Initialize(frame.uploadBuffer, pUploadAddress, kUploadBufferByteSize);
		}

		m_barrier.Initialize(kBarrierCapacity);
		m_fence.Initialize(device, 0);
		m_swapchain.Initialize(device, m_pWindow, m_pCommandQueue, { .latency = kFramesInFlight });

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
			device.CreateRTV(frame.backBufferRTV, m_swapchain.GetBackBuffer(i), {});

			frame.backBufferHandle = RegisterTexture(&m_swapchain.GetBackBuffer(i), EResourceState::Present);
		}

		m_hdrTargetRTV = m_rtvHeap.Allocate();
		m_hdrTargetSRV = m_srvHeap.Allocate();
		device.CreateRTV(m_hdrTargetRTV, m_hdrTarget, {});
		device.CreateSRV(m_hdrTargetSRV, m_hdrTarget);
		m_hdrTargetHandle = RegisterTexture(&m_hdrTarget, EResourceState::ShaderResource);

		m_depthBufferDSV = m_dsvHeap.Allocate();
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
		m_depthBufferHandle = RegisterTexture(&m_depthBuffer, EResourceState::DepthStencil);

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		m_passContext.pRenderer = this;
	}

	dVector<RenderPass>::iterator Renderer::FindRenderPass(RenderTypeID typeID)
	{
		return std::find_if(m_passes.begin(), m_passes.end(), [=](const RenderPass& pass) { return pass.typeID == typeID; });
	}

	void Renderer::DestroyRenderPass(RenderPass& pass)
	{
		for (const Frame& frame : m_frames)
			WaitForFrame(frame);
		pass.pShutdown(*this, pass.pData);
	}

	void Renderer::ResolvePipelines(RenderPass& pass)
	{
		if (!pass.pGetPipelineDescs)
			return;
		for (const PipelineDesc& desc : pass.pGetPipelineDescs())
			pass.pipelines.push_back(m_pPSOCache->Resolve(desc));
	}

	PSO* Renderer::ResolvePSO(RenderPass& pass, MaterialKey key)
	{
		if (!pass.psos[key] && pass.pGetPipelineDesc && (!pass.pIsPassMaterial || pass.pIsPassMaterial(key)))
			pass.psos[key] = m_pPSOCache->Resolve(pass.pGetPipelineDesc(key));
		return pass.psos[key];
	}

	void Renderer::ResolvePSOs(MaterialKey key)
	{
		for (RenderPass& pass : m_passes)
			ResolvePSO(pass, key);
	}

	void Renderer::Destroy()
	{
		for (Frame& frame : m_frames)
		{
			WaitForFrame(frame);
			for (Buffer& buffer : frame.buffersToRelease)
				buffer.Destroy();
			frame.buffersToRelease.clear();
			m_rtvHeap.Free(frame.backBufferRTV);
			frame.uploadBuffer.Destroy();
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
			frame.srvHeap.Destroy();
			frame.samplerHeap.Destroy();
		}
		m_dsvHeap.Free(m_depthBufferDSV);
		m_rtvHeap.Free(m_hdrTargetRTV);
		m_srvHeap.Free(m_hdrTargetSRV);

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
		m_hdrTarget.Destroy();
		m_depthBuffer.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();
	}

	ResourceHandle Renderer::CreateTexture(const TextureDesc& desc)
	{
		Texture* pTexture = new Texture();
		pTexture->Initialize(*GetDevice(), desc);

		const dU32 subresourceCount = desc.dimensions[2] * desc.mipLevels;
		ResourceHandle handle = RegisterTexture(pTexture, desc.initialState, subresourceCount);
		m_resources[handle].isExternal = false;
		return handle;
	}

	ResourceHandle Renderer::CreateBuffer(const BufferDesc& desc)
	{
		Buffer* pBuffer = new Buffer();
		pBuffer->Initialize(*GetDevice(), desc);

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

	void Renderer::OnResize(dU32 width, dU32 height)
	{
		Device& device = *GetDevice();
		for (Frame& f : m_frames)
			WaitForFrame(f);

		m_swapchain.Resize(width, height);
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < kFramesInFlight; i++)
		{
			device.CreateRTV(m_frames[i].backBufferRTV, m_swapchain.GetBackBuffer(i), {});
			SetPhysicalResource(m_frames[i].backBufferHandle, &m_swapchain.GetBackBuffer(i), EResourceState::Present);
		}

		TextureDesc hdrTargetDesc
		{
			.debugName = L"HDRTarget",
			.usage{ ETextureUsage::RenderTarget | ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ EFormat::R16G16B16A16_FLOAT },
			.initialState{ EResourceState::ShaderResource },
		};

		m_hdrTarget.Destroy();
		m_hdrTarget.Initialize(device, hdrTargetDesc);
		device.CreateSRV(m_hdrTargetSRV, m_hdrTarget);
		device.CreateRTV(m_hdrTargetRTV, m_hdrTarget, {});
		SetPhysicalResource(m_hdrTargetHandle, &m_hdrTarget, EResourceState::ShaderResource);

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

	Frame& Renderer::PrepareFrame()
	{
		Frame& frame = m_frames[m_frameIndex];
		WaitForFrame(frame);
		for (Buffer& buffer : frame.buffersToRelease)
			buffer.Destroy();
		frame.buffersToRelease.clear();
		frame.staging.Reset();
		return frame;
	}

	StagingMemory Renderer::AllocateStaging(dU64 byteSize, dU64 alignment)
	{
		Frame& frame = m_frames[m_frameIndex];
		const StagingMemory staging = frame.staging.Allocate(byteSize, alignment);
		if (staging.IsValid())
			return staging;

		Buffer& buffer = frame.buffersToRelease.emplace_back();
		buffer.Initialize(*m_pDevice, { .debugName = L"StagingFallback", .memory = EBufferMemory::CPU, .byteSize = (dU32)byteSize });
		void* pCpuAddress{ nullptr };
		buffer.Map(0, 0, &pCpuAddress);
		return { .buffer = buffer, .offset = 0, .pCpuAddress = pCpuAddress };
	}

	void Renderer::Render(const FrameData& frameData, const Camera& camera)
	{
		Device& device = *GetDevice();
		Frame& frame = PrepareFrame();
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(frame.srvHeap, frame.samplerHeap);
		frame.srvHeap.Reset();
		frame.samplerHeap.Reset();

		Assert(frame.srvHeap.GetCapacity() >= m_srvHeap.GetCapacity() + frameData.sharedSRVHeapCapacity + kTransientSRVCapacity);
		dU32 sharedSRVCapacity = frameData.sharedSRVHeapCapacity;
		device.CopyDescriptors(sharedSRVCapacity, frameData.sharedSRVHeapCPUAddress, frame.srvHeap.GetCPUAddress(), EDescriptorHeapType::SRV_CBV_UAV);
		device.CopyDescriptors(m_srvHeap.GetCapacity(), m_srvHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress() + frameData.sharedSRVHeapCapacity * frame.srvHeap.GetDescriptorSize(), EDescriptorHeapType::SRV_CBV_UAV);
		frame.srvHeap.Allocate(sharedSRVCapacity + m_srvHeap.GetCapacity());

		m_passContext.pFrameData = &frameData;
		m_passContext.pCamera = &camera;
		m_passContext.blackboard.Reset();

		bool usedKeys[Material::kKeyTableSize]{};
		for (const DrawItem& drawItem : frameData.drawItems)
			usedKeys[drawItem.materialKey] = true;
		for (MaterialKey key = 0; key < Material::kKeyTableSize; key++)
		{
			if (usedKeys[key])
				ResolvePSOs(key);
		}

		for (RenderPass& pass : m_passes)
		{
			pass.builder.Reset();
			m_passContext.pPass = &pass;
			pass.pSetup(pass.builder, m_passContext, pass.pData);
		}

#ifdef _DEBUG
		dVector<bool> writtenResources(m_resources.size(), false);
		for (const RenderPass& pass : m_passes)
		{
			for (const ResourceAccess& read : pass.builder.GetReads())
				Assert(writtenResources[read.handle]);
			for (const ResourceAccess& write : pass.builder.GetWrites())
				writtenResources[write.handle] = true;
		}
#endif

		for (RenderPass& pass : m_passes)
		{
			if (pass.builder.IsEmpty())
				continue;

			for (const ResourceAccess& access : pass.builder.GetReads())
				TransitionResource(access);
			for (const ResourceAccess& access : pass.builder.GetWrites())
				TransitionResource(access);
			FlushBarriers(frame.commandList);

			m_passContext.pPass = &pass;
			pass.pExecute(m_passContext, pass.pData);
		}

		if (m_pImGui)
		{
			frame.commandList.SetDescriptorHeaps(m_srvImGuiHeap);
			m_pImGui->Render(frame.commandList);
		}

		TransitionResource({ .handle = frame.backBufferHandle, .state = EResourceState::Present });
		FlushBarriers(frame.commandList);

		frame.commandList.Close();
		m_pCommandQueue->ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_pCommandQueue->Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
	}
}
