#pragma once
#include "Dune/Common/Handle.h"

namespace Dune
{
	template<typename T, typename H = T> 
	class Pool
	{
	public:
		Pool() = default;
		~Pool()
		{
			Assert(m_size == 0);
		}

		DISABLE_COPY_AND_MOVE(Pool);

		void Initialize(dSizeT initialSize)
		{
			Assert(m_size == 0 && !m_generations && !m_datas && !m_freeHandles);
			Assert(initialSize != 0);

			m_size = initialSize;

			m_datas = reinterpret_cast<T*>(::operator new (m_size * sizeof(T)));
			m_generations = new ID::GenerationType[initialSize];
			m_freeHandles = new ID::IDType[initialSize];

			memset(m_generations, 0, sizeof(ID::GenerationType) * m_size);

			for (size_t i{ 0 }; i < m_size; i++)
			{
				m_freeHandles[i] = ID::IDType(i);
			}

			m_nextFreeHandlePosition = m_size - 1;
		}

		void Release()
		{
			Assert(m_nextFreeHandlePosition == (m_size - 1));
			::operator delete[](m_datas);
			delete[] m_freeHandles;
			delete[] m_generations;

			m_size = 0;
			m_nextFreeHandlePosition = 0;
			m_datas = nullptr;
			m_freeHandles = nullptr;
			m_generations = nullptr;
		}

		template <typename... Args>
		[[nodiscard]] Handle<H> Create(Args&&... args)
		{
			Assert(m_size != 0);

			if (m_nextFreeHandlePosition >= m_size)
			{
				Resize();
			}

			Handle<H> handle{};
			handle.m_id = m_freeHandles[m_nextFreeHandlePosition];
			m_nextFreeHandlePosition--;

			new (m_datas + ID::GetIndex(handle.m_id)) T(std::forward<Args>(args)...);

			return handle;
		}

		void Remove(Handle<H> handle)
		{
			Assert(m_size != 0);
			Assert(m_nextFreeHandlePosition != (m_size - 1));
			Assert(IsValid(handle));

			ID::IDType index{ ID::GetIndex(handle.m_id) };
			ID::IDType newID = ID::NextGeneration(handle.m_id);
			
			// Add handle
			m_nextFreeHandlePosition++;
			m_freeHandles[m_nextFreeHandlePosition] = newID;

			// Invalidate previous handle
			m_generations[index] = ID::GetGeneration(newID);
			
			// Destroy T
			m_datas[index].~T();
		}

		[[nodiscard]] bool IsValid(Handle<H> handle) const
		{
			Assert(m_size != 0);

			ID::IDType index{ ID::GetIndex(handle.m_id) };
			ID::GenerationType generation{ ID::GetGeneration(handle.m_id) };

			return handle.IsValid() && index < m_size && (m_generations[index] == generation);
		}

		[[nodiscard]] T& Get(Handle<H> handle)
		{
			Assert(IsValid(handle));

			return m_datas[ID::GetIndex(handle.m_id)];
		}

		[[nodiscard]] const T& Get(Handle<H> handle) const
		{
			Assert(IsValid(handle));

			return m_datas[ID::GetIndex(handle.m_id)];
		}

	private:
		void Resize()
		{
			Assert(m_size != 0);

			dSizeT newSize{ m_size * 2 };

			T* newData{ reinterpret_cast<T*>(::operator new (newSize * sizeof(T))) };
			memcpy(newData, m_datas, sizeof(T) * m_size);
			::operator delete[](m_datas);
			m_datas = newData;

			ID::IDType* newFreeHandles{ new ID::IDType[newSize] };
			memcpy(&newFreeHandles[m_size], m_freeHandles, sizeof(ID::IDType) * m_size);
			for (dSizeT i{ 0 }; i < m_size; i++)
			{
				newFreeHandles[i] = ID::IDType(i + m_size);
			}
			delete[] m_freeHandles;
			m_freeHandles = newFreeHandles;

			ID::GenerationType* newGenerations{ new ID::GenerationType[newSize] };
			memcpy(newGenerations, m_generations, sizeof(ID::GenerationType) * (m_size));
			memset(&newGenerations[m_size], 0, sizeof(ID::GenerationType) * (m_size));
			delete[] m_generations;
			m_generations = newGenerations;

			m_nextFreeHandlePosition = m_size - 1;
			m_size = newSize;
		}

	private:
		dSizeT						m_size{0};
		dSizeT						m_nextFreeHandlePosition{0};
		ID::IDType*					m_freeHandles{nullptr};
		ID::GenerationType*			m_generations{nullptr};
		T*							m_datas{nullptr};
		
	};
}
