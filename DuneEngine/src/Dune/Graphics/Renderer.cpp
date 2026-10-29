#include "pch.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RendererDesc.h"
#include "Dune/Platform/Window.h"
#include <Dune/Graphics/FrameData.h>
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Graphics/PSOCache.h"

namespace Dune::Graphics
{
	void Renderer::Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, PSOCache& psoCache, DescriptorAllocator& srvAllocator, Window& window, const RendererDesc& desc)
	{
		m_pDevice = &device;
		m_pCommandQueue = &commandQueue;
		m_pPSOCache = &psoCache;
		m_pSRVAllocator = &srvAllocator;
		m_pWindow = &window;

		m_resolution = { window.GetWidth(), window.GetHeight() };

		for (Frame& frame : m_frames)
		{
			frame.commandAllocator.Initialize(device, RHI::ECommandType::Direct);
			frame.commandList.Initialize(device, RHI::ECommandType::Direct, frame.commandAllocator);
			frame.commandList.Close();
			frame.uploadBuffer.Initialize(device, { .debugName = L"UploadBuffer", .memory = RHI::EBufferMemory::CPU, .byteSize = kUploadBufferByteSize });
			void* pUploadAddress{ nullptr };
			frame.uploadBuffer.Map(0, 0, &pUploadAddress);
			frame.pUploadAddress = (dU8*)pUploadAddress;
			frame.staging.Initialize(kUploadBufferByteSize);
		}

		m_barrier.Initialize(kBarrierCapacity);
		m_fence.Initialize(device, 0);
		m_swapchain.Initialize(device, m_pWindow, m_pCommandQueue, { .latency = kFramesInFlight });

		RHI::DescriptorHeapDesc heapDesc { .type = RHI::EDescriptorHeapType::SRV_CBV_UAV, .capacity = kCPUSRVCapacity, .isShaderVisible = false };
		m_cpuSRVHeap.Initialize(device, heapDesc);
		m_cpuSRVAllocator.Initialize(m_cpuSRVHeap, 0, m_cpuSRVHeap.GetCapacity());

		heapDesc.capacity = 64;
		heapDesc.type = RHI::EDescriptorHeapType::RTV;
		m_rtvHeap.Initialize(device, heapDesc);
		m_rtvAllocator.Initialize(m_rtvHeap, 0, m_rtvHeap.GetCapacity());

		heapDesc.type = RHI::EDescriptorHeapType::DSV;
		m_dsvHeap.Initialize(device, heapDesc);
		m_dsvAllocator.Initialize(m_dsvHeap, 0, m_dsvHeap.GetCapacity());

		m_renderGraph.Initialize(device, *m_pSRVAllocator, m_rtvAllocator, m_dsvAllocator, kFramesInFlight);
		for (dU32 i = 0; i < kFramesInFlight; i++)
			m_frames[i].backBufferHandle = m_renderGraph.RegisterTexture(&m_swapchain.GetBackBuffer(i), RHI::EResourceState::Present);

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
			frame.uploadBuffer.Destroy();
			frame.commandList.Destroy();
			frame.commandAllocator.Destroy();
		}

		for (RenderPass& pass : m_passes)
			pass.pShutdown(*this, pass.pData);
		m_passes.clear();

		for (Frame& frame : m_frames)
		{
			m_pSRVAllocator->Free(frame.srvsToRelease);
			frame.srvsToRelease.clear();
		}

		m_renderGraph.Destroy();

		m_cpuSRVAllocator.Destroy();
		m_rtvAllocator.Destroy();
		m_dsvAllocator.Destroy();
		m_cpuSRVHeap.Destroy();
		m_rtvHeap.Destroy();
		m_dsvHeap.Destroy();
		m_barrier.Destroy();
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
		for (Frame& f : m_frames)
			WaitForFrame(f);

		m_swapchain.Resize(width, height);
		m_resolution = { width, height };
		m_frameIndex = m_swapchain.GetCurrentBackBufferIndex();
		for (dU32 i = 0; i < kFramesInFlight; i++)
			m_renderGraph.SetPhysicalResource(m_frames[i].backBufferHandle, &m_swapchain.GetBackBuffer(i), RHI::EResourceState::Present);
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
		m_pSRVAllocator->Free(frame.srvsToRelease);
		frame.srvsToRelease.clear();
		m_renderGraph.BeginFrame(m_frameIndex);
		frame.staging.Reset();
		return frame;
	}

	StagingMemory Renderer::AllocateStaging(dU64 byteSize, dU64 alignment)
	{
		Frame& frame = m_frames[m_frameIndex];
		const dU64 offset = frame.staging.Allocate(byteSize, alignment);
		if (offset != ScratchAllocator::kInvalidOffset)
			return { .buffer = frame.uploadBuffer, .offset = offset, .pCpuAddress = frame.pUploadAddress + offset };

		RHI::Buffer& buffer = frame.buffersToRelease.emplace_back();
		buffer.Initialize(*m_pDevice, { .debugName = L"StagingFallback", .memory = RHI::EBufferMemory::CPU, .byteSize = (dU32)byteSize });
		void* pCpuAddress{ nullptr };
		buffer.Map(0, 0, &pCpuAddress);
		return { .buffer = buffer, .offset = 0, .pCpuAddress = pCpuAddress };
	}

	void Renderer::Render(const FrameData& frameData, const ViewData& view)
	{
		Frame& frame = PrepareFrame();
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(m_pSRVAllocator->GetHeap());

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
			pass.builder.Reset(m_renderGraph);
			m_passContext.pPass = &pass;
			pass.pSetup(pass.builder, m_passContext, pass.pData);
			Assert(pass.builder.HasExecute() || pass.builder.IsEmpty());
		}
		m_renderGraph.ResolveViews();

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
		m_renderGraph.EndFrame();

		frame.commandList.Close();
		m_pCommandQueue->ExecuteCommandLists(&frame.commandList, 1);
		m_swapchain.Present();
		m_pCommandQueue->Signal(m_fence, ++m_frameCount);
		frame.fenceValue = m_frameCount;
		m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
	}
}
