#pragma once

namespace Dune
{
	template<typename T, dU32 kChunkSize = 1024, dU32 kMaxChunks = 1024>
	class ChunkedArray
	{
		static_assert((kChunkSize & (kChunkSize - 1)) == 0);

	public:
		static constexpr dU32 kCapacity{ kChunkSize * kMaxChunks };

		ChunkedArray() = default;
		ChunkedArray(const ChunkedArray&) = delete;
		ChunkedArray& operator=(const ChunkedArray&) = delete;

		~ChunkedArray()
		{
			Destroy();
		}

		void Destroy()
		{
			for (std::atomic<Chunk*>& chunk : m_chunks)
				delete chunk.exchange(nullptr, std::memory_order_relaxed);
		}

		[[nodiscard]] T& Resolve(dU32 index)
		{
			Assert(index < kCapacity);
			std::atomic<Chunk*>& chunk = m_chunks[index / kChunkSize];
			Chunk* pChunk = chunk.load(std::memory_order_acquire);
			if (!pChunk)
			{
				Chunk* pNewChunk = new Chunk();
				if (chunk.compare_exchange_strong(pChunk, pNewChunk, std::memory_order_acq_rel, std::memory_order_acquire))
					pChunk = pNewChunk;
				else
					delete pNewChunk;
			}
			return pChunk->items[index % kChunkSize];
		}

		[[nodiscard]] T* TryGet(dU32 index)
		{
			if (index >= kCapacity)
				return nullptr;
			Chunk* pChunk = m_chunks[index / kChunkSize].load(std::memory_order_acquire);
			return pChunk ? &pChunk->items[index % kChunkSize] : nullptr;
		}

		[[nodiscard]] const T* TryGet(dU32 index) const
		{
			if (index >= kCapacity)
				return nullptr;
			const Chunk* pChunk = m_chunks[index / kChunkSize].load(std::memory_order_acquire);
			return pChunk ? &pChunk->items[index % kChunkSize] : nullptr;
		}

		[[nodiscard]] T& operator[](dU32 index) { T* pItem = TryGet(index); Assert(pItem); return *pItem; }
		[[nodiscard]] const T& operator[](dU32 index) const { const T* pItem = TryGet(index); Assert(pItem); return *pItem; }

	private:
		struct Chunk
		{
			T items[kChunkSize]{};
		};

		std::atomic<Chunk*> m_chunks[kMaxChunks]{};
	};
}
