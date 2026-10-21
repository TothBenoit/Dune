#include "pch.h"
#include "Dune/Graphics/UploadContext.h"
#include "Dune/Platform/RHI/Device.h"

namespace Dune::Graphics
{
	void UploadContext::Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, const UploadContextDesc& desc)
	{
		Assert(desc.batchCount > 0);
		m_pDevice = &device;
		m_pCommandQueue = &commandQueue;
		m_fence.Initialize(device, 0);

		m_batches.resize(desc.batchCount);
		for (Batch& batch : m_batches)
		{
			batch.commandAllocator.Initialize(device, RHI::ECommandType::Direct);
			batch.commandList.Initialize(device, RHI::ECommandType::Direct, batch.commandAllocator);
			batch.commandList.Close();
			batch.stagingBuffer.Initialize(device, { .debugName = L"UploadStaging", .memory = RHI::EBufferMemory::CPU, .byteSize = (dU32)desc.stagingByteSize });
			void* pCpuAddress{ nullptr };
			batch.stagingBuffer.Map(0, 0, &pCpuAddress);
			batch.staging.Initialize(batch.stagingBuffer, pCpuAddress, desc.stagingByteSize);
		}
	}

	void UploadContext::Destroy()
	{
		Submit();
		if (m_fence.GetValue() < m_fenceValue)
			m_fence.Wait(m_fenceValue);

		for (Batch& batch : m_batches)
		{
			for (RHI::Buffer& buffer : batch.buffersToRelease)
				buffer.Destroy();
			batch.stagingBuffer.Destroy();
			batch.commandList.Destroy();
			batch.commandAllocator.Destroy();
		}
		m_batches.clear();
		m_fence.Destroy();
	}

	UploadContext::Batch& UploadContext::PrepareBatch()
	{
		Batch& batch = m_batches[m_batchIndex];
		if (!m_isPrepared)
		{
			if (m_fence.GetValue() < batch.fenceValue)
				m_fence.Wait(batch.fenceValue);
			for (RHI::Buffer& buffer : batch.buffersToRelease)
				buffer.Destroy();
			batch.buffersToRelease.clear();
			batch.staging.Reset();
			batch.commandAllocator.Reset();
			m_isPrepared = true;
		}
		return batch;
	}

	RHI::StagingMemory UploadContext::AllocateStaging(dU64 byteSize, dU64 alignment)
	{
		Batch& batch = PrepareBatch();
		const RHI::StagingMemory staging = batch.staging.Allocate(byteSize, alignment);
		if (staging.IsValid())
			return staging;

		RHI::Buffer& buffer = batch.buffersToRelease.emplace_back();
		buffer.Initialize(*m_pDevice, { .debugName = L"UploadStagingFallback", .memory = RHI::EBufferMemory::CPU, .byteSize = (dU32)byteSize });
		void* pCpuAddress{ nullptr };
		buffer.Map(0, 0, &pCpuAddress);
		return { .buffer = buffer, .offset = 0, .pCpuAddress = pCpuAddress };
	}

	RHI::CommandList& UploadContext::GetCommandList()
	{
		Batch& batch = PrepareBatch();
		if (!m_isRecording)
		{
			batch.commandList.Reset(batch.commandAllocator);
			m_isRecording = true;
		}
		return batch.commandList;
	}

	void UploadContext::Submit()
	{
		if (!m_isRecording)
			return;

		Batch& batch = m_batches[m_batchIndex];
		batch.commandList.Close();
		m_pCommandQueue->ExecuteCommandLists(&batch.commandList, 1);
		m_pCommandQueue->Signal(m_fence, ++m_fenceValue);
		batch.fenceValue = m_fenceValue;

		m_isRecording = false;
		m_isPrepared = false;
		m_batchIndex = (m_batchIndex + 1) % (dU32)m_batches.size();
	}
}
