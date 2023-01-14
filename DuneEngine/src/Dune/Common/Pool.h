#pragma once
#include "Dune/Common/Handle.h"

namespace Dune
{
	template<typename T, typename H = T> 
	class Pool
	{
	public:
		Pool() = default;
		~Pool() = default;
		DISABLE_COPY_AND_MOVE(Pool);

		template <typename... Args>
		Handle<H> Create(Args&&... args)
		{
			ID::IDType id{};
			
			if (m_freeHandles.size() > 0)
			{
				id = m_freeHandles.front();
				m_freeHandles.pop();
			}
			else
			{
				Assert(m_generations.size() < ID::GetMaximumIndex());
				id = (ID::IDType)m_generations.size();
				m_generations.push_back(ID::GenerationType{ 0 });
				m_datas.push_back(T{});
			}
			
			m_datas[ID::GetIndex(id)].Initialize(std::forward<Args>(args)...);

			return Handle<H>{id};
		}

		void Remove(Handle<H> handle)
		{
			Assert(IsValid(handle));

			ID::IDType index{ ID::GetIndex(handle.m_id) };

			// Generate new ID
			ID::IDType newID = ID::NextGeneration(handle.m_id);
			m_freeHandles.push(newID);

			// Invalidate previous ID
			m_generations[index] = ID::GetGeneration(newID);
			
			// Release T
			m_datas[index].Release();
		}

		bool IsValid(Handle<H> handle) const
		{
			ID::IDType index{ ID::GetIndex(handle.m_id) };
			ID::GenerationType generation{ ID::GetGeneration(handle.m_id) };

			return handle.IsValid() && (index < m_generations.size() ) && (m_generations[index] == generation);
		}

		T& Get(Handle<H> handle)
		{
			Assert(IsValid(handle));

			return m_datas[ID::GetIndex(handle.m_id)];
		}

	private:
		dVector<T>					m_datas;
		dVector<ID::GenerationType> m_generations;
		dQueue<ID::IDType>			m_freeHandles;
	};
}
