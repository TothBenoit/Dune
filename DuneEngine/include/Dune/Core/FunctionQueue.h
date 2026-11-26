#pragma once

#include <mutex>

namespace Dune
{
	class FunctionQueue
	{
	public:
		FunctionQueue() = default;
		FunctionQueue(const FunctionQueue&) = delete;
		FunctionQueue& operator=(const FunctionQueue&) = delete;
		~FunctionQueue() { Assert(m_entries.empty()); }

		template<typename TFunction>
		void Push(TFunction&& function);
		void Execute();

	private:
		struct Entry
		{
			void (*pExecute)(void* pFunction);
			void* pFunction;
		};

	private:
		std::mutex     m_mutex;
		dVector<Entry> m_entries;
	};

	template<typename TFunction>
	void FunctionQueue::Push(TFunction&& function)
	{
		using T = std::decay_t<TFunction>;
		T* pFunction = new T(std::forward<TFunction>(function));
		std::lock_guard lock(m_mutex);
		m_entries.push_back({ [](void* p) { T* pFunction = (T*)p; (*pFunction)(); delete pFunction; }, pFunction });
	}
}
