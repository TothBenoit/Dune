#pragma once

#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/Fence.h"
#include "Dune/Graphics/StagingMemory.h"
#include "Dune/Core/ScratchAllocator.h"

namespace Dune::RHI
{
	class Device;
}

namespace Dune::Graphics
{
	struct UploadContextDesc
	{
		dU64 stagingByteSize{ 16 * 1024 * 1024 };
		dU32 batchCount{ 3 };
	};

	class UploadContext
	{
	public:
		void Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, const UploadContextDesc& desc);
		void Destroy();

		[[nodiscard]] StagingMemory AllocateStaging(dU64 byteSize, dU64 alignment);
		[[nodiscard]] RHI::CommandList& GetCommandList();
		void Submit();

	private:
		struct Batch
		{
			RHI::CommandAllocator commandAllocator;
			RHI::CommandList commandList;
			RHI::Buffer stagingBuffer;
			dU8* pStagingAddress{ nullptr };
			ScratchAllocator staging;
			dVector<RHI::Buffer> buffersToRelease;
			dU64 fenceValue{ 0 };
		};

		Batch& PrepareBatch();

	private:
		RHI::Device* m_pDevice{ nullptr };
		RHI::CommandQueue* m_pCommandQueue{ nullptr };
		RHI::Fence m_fence{};
		dU64 m_fenceValue{ 0 };
		dVector<Batch> m_batches;
		dU32 m_batchIndex{ 0 };
		bool m_isPrepared{ false };
		bool m_isRecording{ false };
	};
}
