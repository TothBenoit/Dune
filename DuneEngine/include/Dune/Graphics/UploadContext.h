#pragma once

#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Fence.h"
#include "Dune/Graphics/RHI/StagingMemory.h"
#include "Dune/Graphics/ScratchAllocator.h"

namespace Dune::Graphics
{
	class Device;

	struct UploadContextDesc
	{
		dU64 stagingByteSize{ 16 * 1024 * 1024 };
		dU32 batchCount{ 3 };
	};

	class UploadContext
	{
	public:
		void Initialize(Device& device, CommandQueue& commandQueue, const UploadContextDesc& desc);
		void Destroy();

		[[nodiscard]] StagingMemory AllocateStaging(dU64 byteSize, dU64 alignment);
		[[nodiscard]] CommandList& GetCommandList();
		void Submit();

	private:
		struct Batch
		{
			CommandAllocator commandAllocator;
			CommandList commandList;
			Buffer stagingBuffer;
			ScratchAllocator staging;
			dVector<Buffer> buffersToRelease;
			dU64 fenceValue{ 0 };
		};

		Batch& PrepareBatch();

	private:
		Device* m_pDevice{ nullptr };
		CommandQueue* m_pCommandQueue{ nullptr };
		Fence m_fence{};
		dU64 m_fenceValue{ 0 };
		dVector<Batch> m_batches;
		dU32 m_batchIndex{ 0 };
		bool m_isPrepared{ false };
		bool m_isRecording{ false };
	};
}
