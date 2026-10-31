#include "pch.h"
#include "Dune/Graphics/Renderer.h"
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

		RHI::DescriptorHeapDesc heapDesc { .type = RHI::EDescriptorHeapType::RTV, .capacity = 64, .isShaderVisible = false };
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

		m_desc = desc;
		m_pPipelineData = m_desc.pCreate(*this);
	}

	void Renderer::InitializeImGui(const char* pLayoutPath)
	{
		m_imgui.Initialize({ .pWindow = m_pWindow, .pDevice = m_pDevice, .pCommandQueue = m_pCommandQueue, .framesInFlight = kFramesInFlight, .renderTargetFormat = m_swapchain.GetBackBuffer(0).GetFormat(), .pLayoutPath = pLayoutPath });
	}

	void Renderer::AddRenderPass(const RenderPass& pass)
	{
		m_passes.push_back(pass);
		if (pass.typeIndex >= m_passPSOs.size())
			m_passPSOs.resize(pass.typeIndex + 1);
		ResolvePipelines(m_passes.back());
	}

	void Renderer::ResolvePipelines(RenderPass& pass)
	{
		dVector<PSO*>& pipelines = m_passPSOs[pass.typeIndex].pipelines;
		if (!pass.pGetPipelineDescs || !pipelines.empty())
			return;
		for (const PipelineDesc& desc : pass.pGetPipelineDescs())
			pipelines.push_back(m_pPSOCache->Resolve(desc));
	}

	PSO* Renderer::ResolvePSO(RenderPass& pass, MaterialKey key)
	{
		PSO*& pPSO = m_passPSOs[pass.typeIndex].materials[key];
		if (!pPSO && pass.pGetPipelineDesc && (!pass.pIsPassMaterial || pass.pIsPassMaterial(key)))
			pPSO = m_pPSOCache->Resolve(pass.pGetPipelineDesc(key));
		return pPSO;
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
			frame.fenceValue = 0;
		}
		m_frameCount = 0;

		for (RenderPass& pass : m_passes)
			pass.pShutdown(*this, pass.pData);
		m_desc.pDestroy(*this, m_pPipelineData);
		m_passes.clear();
		m_passPSOs.clear();

		m_renderGraph.Destroy();

		m_rtvAllocator.Destroy();
		m_dsvAllocator.Destroy();
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

	void Renderer::Render(const FrameContext& frameContext, const FrameData& frameData, const ViewData& view)
	{
		Frame& frame = PrepareFrame();
		frame.commandAllocator.Reset();
		frame.commandList.Reset(frame.commandAllocator);
		frame.commandList.SetDescriptorHeaps(m_pSRVAllocator->GetHeap());

		m_passContext.pFrameContext = &frameContext;
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

		m_desc.pSetup(m_passContext, m_pPipelineData);
		m_renderGraph.ResolveViews();

#ifdef _DEBUG
		dVector<bool> writtenTextures(m_renderGraph.GetTextureCount(), false);
		dVector<bool> writtenBuffers(m_renderGraph.GetBufferCount(), false);
		for (dU32 passIndex = 0; passIndex < m_renderGraph.GetPassCount(); passIndex++)
		{
			const RenderGraphBuilder& builder = m_renderGraph.GetPass(passIndex);
			Assert(builder.HasExecute());
			for (const TextureAccess& read : builder.GetTextureReads())
				Assert(writtenTextures[read.handle.index]);
			for (const BufferAccess& read : builder.GetBufferReads())
				Assert(writtenBuffers[read.handle.index]);
			for (const TextureAccess& write : builder.GetTextureWrites())
				writtenTextures[write.handle.index] = true;
			for (const BufferAccess& write : builder.GetBufferWrites())
				writtenBuffers[write.handle.index] = true;
		}
#endif

		for (dU32 passIndex = 0; passIndex < m_renderGraph.GetPassCount(); passIndex++)
		{
			const RenderGraphBuilder& builder = m_renderGraph.GetPass(passIndex);
			for (const TextureAccess& access : builder.GetTextureReads())
				m_renderGraph.TransitionTexture(access, m_barrier);
			for (const BufferAccess& access : builder.GetBufferReads())
				m_renderGraph.TransitionBuffer(access, m_barrier);
			for (const TextureAccess& access : builder.GetTextureWrites())
				m_renderGraph.TransitionTexture(access, m_barrier);
			for (const BufferAccess& access : builder.GetBufferWrites())
				m_renderGraph.TransitionBuffer(access, m_barrier);
			FlushBarriers(frame.commandList);

			m_passContext.passTypeIndex = builder.GetTypeIndex();
			builder.Execute(m_passContext);
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
