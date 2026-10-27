#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RendererDesc.h"
#include "Dune/Platform/Window.h"
#include <Dune/Graphics/FrameData.h>
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/PSOCache.h"

namespace Dune::Graphics
{
	void Renderer::Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, PSOCache& psoCache, Window& window, const RendererDesc& desc)
	{
		m_pDevice = &device;
		m_pCommandQueue = &commandQueue;
		m_pPSOCache = &psoCache;
		m_pWindow = &window;
		m_renderGraph.Initialize(device);

		m_resolution = { window.GetWidth(), window.GetHeight() };
		m_depthBuffer.Initialize(device,
			{
				.debugName = L"DepthBuffer",
				.usage = RHI::ETextureUsage::DepthStencil,
				.dimensions = { m_resolution.x, m_resolution.y, 1 },
				.format = RHI::EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = RHI::EResourceState::DepthStencil
			});

		RHI::TextureDesc hdrTargetDesc
		{
			.debugName = L"HDRTarget",
			.usage{ RHI::ETextureUsage::RenderTarget | RHI::ETextureUsage::ShaderResource },
			.dimensions = { m_resolution.x, m_resolution.y, 1 },
			.mipLevels{ 1 },
			.format{ RHI::EFormat::R16G16B16A16_FLOAT },
			.initialState{ RHI::EResourceState::ShaderResource },
		};
		m_hdrTarget.Initialize(device, hdrTargetDesc);

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(device, RHI::ECommandType::Direct);
			frame.commandList.Initialize(device, RHI::ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
			frame.srvHeap.Initialize(device, { .type = RHI::EDescriptorHeapType::SRV_CBV_UAV, .capacity = ResourceManager::kSharedSRVCapacity + kPersistentSRVCapacity + kTransientSRVCapacity, .isShaderVisible = true });
			frame.samplerHeap.Initialize(device, { .type = RHI::EDescriptorHeapType::Sampler, .capacity = 64, .isShaderVisible = true });
			frame.uploadBuffer.Initialize(device, { .debugName = L"UploadBuffer", .memory = RHI::EBufferMemory::CPU, .byteSize = kUploadBufferByteSize });
			void* pUploadAddress{ nullptr };
			frame.uploadBuffer.Map(0, 0, &pUploadAddress);
			frame.staging.Initialize(frame.uploadBuffer, pUploadAddress, kUploadBufferByteSize);
		}

		m_barrier.Initialize(kBarrierCapacity);
		m_fence.Initialize(device, 0);
		m_swapchain.Initialize(device, m_pWindow, m_pCommandQueue, { .latency = kFramesInFlight });

		RHI::DescriptorHeapDesc heapDesc { .type = RHI::EDescriptorHeapType::SRV_CBV_UAV, .capacity = kPersistentSRVCapacity, .isShaderVisible = false };
		m_srvHeap.Initialize(device, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = RHI::EDescriptorHeapType::RTV;
		m_rtvHeap.Initialize(device, heapDesc);

		heapDesc.type = RHI::EDescriptorHeapType::DSV;
		m_dsvHeap.Initialize(device, heapDesc);

		for (dU32 i = 0; i < kFramesInFlight; i++)
		{
			Frame& frame = m_frames[i];
			frame.backBufferRTV = m_rtvHeap.Allocate();
			device.CreateRTV(frame.backBufferRTV, m_swapchain.GetBackBuffer(i), {});

			frame.backBufferHandle = m_renderGraph.RegisterTexture(&m_swapchain.GetBackBuffer(i), RHI::EResourceState::Present);
		}

		m_hdrTargetRTV = m_rtvHeap.Allocate();
		m_hdrTargetSRV = m_srvHeap.Allocate();
		device.CreateRTV(m_hdrTargetRTV, m_hdrTarget, {});
		device.CreateSRV(m_hdrTargetSRV, m_hdrTarget);
		m_hdrTargetHandle = m_renderGraph.RegisterTexture(&m_hdrTarget, RHI::EResourceState::ShaderResource);

		m_depthBufferDSV = m_dsvHeap.Allocate();
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
		m_depthBufferHandle = m_renderGraph.RegisterTexture(&m_depthBuffer, RHI::EResourceState::DepthStencil);

		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();

		m_passContext.pRenderer = this;

		m_passes.reserve(desc.passes.size());
		for (const RenderPassDesc& passDesc : desc.passes)
		{
			RenderPass& pass = m_passes.emplace_back(passDesc.pMake(*this));
			ResolvePipelines(pass);
		}
	}

	void Renderer::InitializeImGui(const char* pLayoutPath)
	{
		m_imgui.Initialize({ .pWindow = m_pWindow, .pDevice = m_pDevice, .pCommandQueue = m_pCommandQueue, .framesInFlight = kFramesInFlight, .renderTargetFormat = m_swapchain.GetBackBuffer(0).GetFormat(), .pLayoutPath = pLayoutPath });
	}

	dVector<RenderPass>::iterator Renderer::FindRenderPass(RenderTypeID typeID)
	{
		return std::find_if(m_passes.begin(), m_passes.end(), [=](const RenderPass& pass) { return pass.typeID == typeID; });
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
			WaitForFrame(frame);
		if (HasImGui())
			m_imgui.Destroy();

		for (Frame& frame : m_frames)
		{
			for (RHI::Buffer& buffer : frame.buffersToRelease)
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

		m_renderGraph.Destroy();

		m_srvHeap.Destroy();
		m_rtvHeap.Destroy();
		m_dsvHeap.Destroy();
		m_barrier.Destroy();
		m_hdrTarget.Destroy();
		m_depthBuffer.Destroy();
		m_swapchain.Destroy();
		m_fence.Destroy();
	}

	void Renderer::FlushBarriers(RHI::CommandList& commandList)
	{
		if (m_barrier.GetBarrierCount() != 0)
		{
			commandList.Transition(m_barrier);
			m_barrier.Reset();
		}
	}

	void Renderer::OnResize(dU32 width, dU32 height)
	{
		RHI::Device& device = *GetDevice();
		for (Frame& f : m_frames)
			WaitForFrame(f);

		m_swapchain.Resize(width, height);
		m_resolution = { width, height };
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < kFramesInFlight; i++)
		{
			device.CreateRTV(m_frames[i].backBufferRTV, m_swapchain.GetBackBuffer(i), {});
			m_renderGraph.SetPhysicalResource(m_frames[i].backBufferHandle, &m_swapchain.GetBackBuffer(i), RHI::EResourceState::Present);
		}

		RHI::TextureDesc hdrTargetDesc
		{
			.debugName = L"HDRTarget",
			.usage{ RHI::ETextureUsage::RenderTarget | RHI::ETextureUsage::ShaderResource },
			.dimensions = { width, height, 1},
			.mipLevels{ 1 },
			.format{ RHI::EFormat::R16G16B16A16_FLOAT },
			.initialState{ RHI::EResourceState::ShaderResource },
		};

		m_hdrTarget.Destroy();
		m_hdrTarget.Initialize(device, hdrTargetDesc);
		device.CreateSRV(m_hdrTargetSRV, m_hdrTarget);
		device.CreateRTV(m_hdrTargetRTV, m_hdrTarget, {});
		m_renderGraph.SetPhysicalResource(m_hdrTargetHandle, &m_hdrTarget, RHI::EResourceState::ShaderResource);

		m_depthBuffer.Destroy();
		m_depthBuffer.Initialize(device,
			{
				.debugName = L"DepthBuffer",
				.usage = RHI::ETextureUsage::DepthStencil,
				.dimensions = { width, height, 1 },
				.format = RHI::EFormat::D32_FLOAT,
				.clearValue = {1.f, 1.f, 1.f, 1.f},
				.initialState = RHI::EResourceState::DepthStencil
			});
		device.CreateDSV(m_depthBufferDSV, m_depthBuffer, {});
		m_renderGraph.SetPhysicalResource(m_depthBufferHandle, &m_depthBuffer, RHI::EResourceState::DepthStencil);
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
		for (RHI::Buffer& buffer : frame.buffersToRelease)
			buffer.Destroy();
		frame.buffersToRelease.clear();
		frame.staging.Reset();
		return frame;
	}

	RHI::StagingMemory Renderer::AllocateStaging(dU64 byteSize, dU64 alignment)
	{
		Frame& frame = m_frames[m_frameIndex];
		const RHI::StagingMemory staging = frame.staging.Allocate(byteSize, alignment);
		if (staging.IsValid())
			return staging;

		RHI::Buffer& buffer = frame.buffersToRelease.emplace_back();
		buffer.Initialize(*m_pDevice, { .debugName = L"StagingFallback", .memory = RHI::EBufferMemory::CPU, .byteSize = (dU32)byteSize });
		void* pCpuAddress{ nullptr };
		buffer.Map(0, 0, &pCpuAddress);
		return { .buffer = buffer, .offset = 0, .pCpuAddress = pCpuAddress };
	}

	void Renderer::Render(const FrameData& frameData, const ViewData& view)
	{
		RHI::Device& device = *GetDevice();
		Frame& frame = PrepareFrame();
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(frame.srvHeap, frame.samplerHeap);
		frame.srvHeap.Reset();
		frame.samplerHeap.Reset();

		Assert(frame.srvHeap.GetCapacity() >= m_srvHeap.GetCapacity() + frameData.pResources->sharedSRVHeapCapacity + kTransientSRVCapacity);
		dU32 sharedSRVCapacity = frameData.pResources->sharedSRVHeapCapacity;
		device.CopyDescriptors(sharedSRVCapacity, frameData.pResources->sharedSRVHeapCPUAddress, frame.srvHeap.GetCPUAddress(), RHI::EDescriptorHeapType::SRV_CBV_UAV);
		device.CopyDescriptors(m_srvHeap.GetCapacity(), m_srvHeap.GetCPUAddress(), frame.srvHeap.GetCPUAddress() + frameData.pResources->sharedSRVHeapCapacity * frame.srvHeap.GetDescriptorSize(), RHI::EDescriptorHeapType::SRV_CBV_UAV);
		frame.srvHeap.Allocate(sharedSRVCapacity + m_srvHeap.GetCapacity());

		m_passContext.pFrameData = &frameData;
		m_passContext.pView = &view;
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
			Assert(pass.builder.HasExecute() || pass.builder.IsEmpty());
		}

#ifdef _DEBUG
		dVector<bool> writtenTextures(m_renderGraph.GetTextureCount(), false);
		dVector<bool> writtenBuffers(m_renderGraph.GetBufferCount(), false);
		for (const RenderPass& pass : m_passes)
		{
			for (const TextureAccess& read : pass.builder.GetTextureReads())
				Assert(writtenTextures[read.handle.index]);
			for (const BufferAccess& read : pass.builder.GetBufferReads())
				Assert(writtenBuffers[read.handle.index]);
			for (const TextureAccess& write : pass.builder.GetTextureWrites())
				writtenTextures[write.handle.index] = true;
			for (const BufferAccess& write : pass.builder.GetBufferWrites())
				writtenBuffers[write.handle.index] = true;
		}
#endif

		for (RenderPass& pass : m_passes)
		{
			if (!pass.builder.HasExecute())
				continue;

			for (const TextureAccess& access : pass.builder.GetTextureReads())
				m_renderGraph.TransitionTexture(access, m_barrier);
			for (const BufferAccess& access : pass.builder.GetBufferReads())
				m_renderGraph.TransitionBuffer(access, m_barrier);
			for (const TextureAccess& access : pass.builder.GetTextureWrites())
				m_renderGraph.TransitionTexture(access, m_barrier);
			for (const BufferAccess& access : pass.builder.GetBufferWrites())
				m_renderGraph.TransitionBuffer(access, m_barrier);
			FlushBarriers(frame.commandList);

			m_passContext.pPass = &pass;
			pass.builder.Execute(m_passContext);
		}

		if (HasImGui())
			m_imgui.Render(frame.commandList);

		m_renderGraph.TransitionTexture({ .handle = frame.backBufferHandle, .state = RHI::EResourceState::Present }, m_barrier);
		FlushBarriers(frame.commandList);

		frame.commandList.Close();
		m_pCommandQueue->ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_pCommandQueue->Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
	}
}
