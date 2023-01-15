#pragma once
#include "Dune/Common/Handle.h"

namespace Dune
{
	template<typename T, typename H = T> 
	class Pool
	{
	public:
		Pool(dSizeT initialSize)
			: m_size(initialSize)
		{
			Assert(initialSize != 0);
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
		~Pool()
		{
			Assert(m_nextFreeHandlePosition == (m_size - 1));
			::operator delete[](m_datas);
			delete[] m_freeHandles;
			delete[] m_generations;
		}
		DISABLE_COPY_AND_MOVE(Pool);

		template <typename... Args>
		[[nodiscard]] Handle<H> Create(Args&&... args)
		{
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
		dSizeT						m_size;
		dSizeT						m_nextFreeHandlePosition;
		ID::IDType*					m_freeHandles;
		ID::GenerationType*			m_generations;
		T*							m_datas;
		
	};
}
