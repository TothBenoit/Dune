#include "pch.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Platform/Fiber.h"
#include <immintrin.h>

namespace Dune::Job
{
	struct CounterInstance;

	class SpinLock
	{
	public:
		void lock();
		void unlock();

	private:
		std::atomic<bool> m_lock{ false };
	};

	// Implementation inspired by WickedEngine blog
	template <typename T, dSizeT capacity>
	class ConcurrentRingBuffer
	{
	public:
		inline bool push_back(T&& item)
		{
			bool result = false;
			lock.lock();
			dSizeT next = (head + 1) % (capacity+1);
			if(next != tail)
			{
				data[head] = std::move(item);
				head = next;
				result = true;
			}
			lock.unlock();
			return result;
		}

		inline bool push_back(const T& item)
		{
			T copy{ item };
			return push_back(std::move(copy));
		}

		inline bool pop_front(T& item)
		{
			bool result = false;
			lock.lock();
			if (tail != head)
			{
				item = std::move(data[tail]);
				tail = (tail + 1) % (capacity+1);
				result = true;
			}
			lock.unlock();
			return result;
		}

	private:
		T data[capacity+1];
		dSizeT head = 0;
		dSizeT tail = 0;
		SpinLock lock;
	};

	struct JobInstance
	{
		std::function<void()> m_executable;
		CounterInstance* m_pCounter;
	};

	struct PendingJob
	{
		JobInstance m_job;
		PendingJob* m_pNext;
	};

	struct FiberDecl
	{
		void* pFiber;
		dU32 threadIndex;
	};

	struct WaitingFiberEntry
	{
		FiberDecl m_fiber;
		WaitingFiberEntry* m_pNext;
	};

	struct WaitingListEntry
	{
		CounterInstance* m_pCounter;
		WaitingListEntry* m_pNext;
	};

	void WakeFiber(const FiberDecl& fiber);
	void PushJob(JobInstance&& job);

	struct CounterInstance
	{
		uint32_t GetValue() const { return m_counter.load(); }

		void Decrement()
		{
			m_lock.lock();
			const uint32_t previous = m_counter.fetch_sub(1);
			Assert(previous > 0);
			const bool hasFinished = previous == 1;
			WaitingFiberEntry* pWaitingFiber = nullptr;
			PendingJob* pPendingJob = nullptr;
			WaitingListEntry* pCurrentWaitingCounter = nullptr;
			if (hasFinished)
			{
				pWaitingFiber = m_pWaitingFibers;
				m_pWaitingFibers = nullptr;
				pPendingJob = m_pPendingJobs;
				m_pPendingJobs = nullptr;
				pCurrentWaitingCounter = m_pWaitingCounters;
				m_pWaitingCounters = nullptr;
			}
			m_lock.unlock();

			if (!hasFinished)
				return;

			while (pWaitingFiber)
			{
				WaitingFiberEntry* pNext = pWaitingFiber->m_pNext;
				WakeFiber(pWaitingFiber->m_fiber);
				pWaitingFiber = pNext;
			}

			while (pPendingJob)
			{
				PendingJob* pNext = pPendingJob->m_pNext;
				PushJob(std::move(pPendingJob->m_job));
				delete pPendingJob;
				pPendingJob = pNext;
			}

			while (pCurrentWaitingCounter)
			{
				WaitingListEntry* pNext = pCurrentWaitingCounter->m_pNext;
				pCurrentWaitingCounter->m_pCounter->Decrement();
				pCurrentWaitingCounter->m_pCounter->Release();
				delete pCurrentWaitingCounter;
				pCurrentWaitingCounter = pNext;
			}
		}

		bool TryAddWaitingFiber(WaitingFiberEntry& entry) const
		{
			m_lock.lock();
			const bool shouldWait = GetValue() > 0;
			if (shouldWait)
			{
				entry.m_pNext = m_pWaitingFibers;
				m_pWaitingFibers = &entry;
			}
			m_lock.unlock();
			return shouldWait;
		}

		bool TryAddPendingJob(PendingJob& pendingJob) const
		{
			m_lock.lock();
			const bool shouldWait = GetValue() > 0;
			if (shouldWait)
			{
				pendingJob.m_pNext = m_pPendingJobs;
				m_pPendingJobs = &pendingJob;
			}
			m_lock.unlock();
			return shouldWait;
		}

		void AddListener(CounterInstance& counter) const
		{
			counter.m_counter.fetch_add(1);
			m_lock.lock();
			bool shouldAdd = GetValue() > 0;
			if (shouldAdd)
			{
				counter.m_refCount.fetch_add(1);
				WaitingListEntry* pEntry = new WaitingListEntry();
				pEntry->m_pCounter = &counter;
				pEntry->m_pNext = m_pWaitingCounters;
				m_pWaitingCounters = pEntry;
			}
			m_lock.unlock();
			if (!shouldAdd)
				counter.Decrement();
		}

		void Release()
		{
			if (m_refCount.fetch_sub(1) == 1)
				delete this;
		}

		~CounterInstance()
		{
			Assert(!m_pWaitingFibers && !m_pPendingJobs && !m_pWaitingCounters);
			WaitingListEntry* pCurrentWaitingCounter = m_pWaitingCounters;
			while (pCurrentWaitingCounter) {
				pCurrentWaitingCounter->m_pCounter->Release();
				WaitingListEntry* pNext = pCurrentWaitingCounter->m_pNext;
				delete pCurrentWaitingCounter;
				pCurrentWaitingCounter = pNext;
			}
		}

		std::atomic<uint32_t> m_counter{ 0 };
		std::atomic<uint32_t> m_refCount{ 1 };
		mutable WaitingListEntry* m_pWaitingCounters{ nullptr };
		mutable WaitingFiberEntry* m_pWaitingFibers{ nullptr };
		mutable PendingJob* m_pPendingJobs{ nullptr };
		mutable SpinLock m_lock;
	};

	const dU32 g_maxFiberPerThread{ 256 };
	const dU32 g_initialFiberPerThread{ 32 };
	const dU32 g_fiberStackCommitSize{ 64 * 1024 };
	const dU32 g_fiberStackReserveSize{ 1024 * 1024 };

	struct Worker
	{
		Worker(void (*pFunc)(dU32))
			: pEntryPoint{ pFunc }
		{}

		void Run(dU32 threadID)
		{
			thread = std::thread(pEntryPoint, threadID);
		}

		bool CreateFiber(dU32 threadID, FiberDecl& outFiber);

		void (*pEntryPoint)(dU32);
		std::thread thread;
		void* fibers[g_maxFiberPerThread]{};
		dU32 fiberCount{ 0 };
		ConcurrentRingBuffer<FiberDecl, g_maxFiberPerThread> freeFibers;
		ConcurrentRingBuffer<FiberDecl, g_maxFiberPerThread> sleepingFibers;
	};

	std::vector<Worker*> g_pWorkers;
	ConcurrentRingBuffer<JobInstance, 16384> g_jobPool;

	thread_local dU32 g_workerID;
	thread_local void* g_pMainFiber{ nullptr };
	thread_local FiberDecl g_currentFiber{ nullptr, 0 };

	std::atomic<uint64_t>   g_currentLabel{ 0 };
	std::atomic<uint64_t>   g_finishedLabel{ 0 };
	std::atomic<bool>       g_workerRunning{ false };

	void WorkerMainLoop(void* pData);

	bool Worker::CreateFiber(dU32 threadID, FiberDecl& outFiber)
	{
		if (fiberCount == g_maxFiberPerThread)
			return false;
		void* pFiber = Fiber::Create(g_fiberStackCommitSize, g_fiberStackReserveSize, &WorkerMainLoop, nullptr);
		Assert(pFiber);
		fibers[fiberCount++] = pFiber;
		outFiber = { pFiber, threadID };
		return true;
	}

	void WakeFiber(const FiberDecl& fiber)
	{
		bool result = g_pWorkers[fiber.threadIndex]->sleepingFibers.push_back(fiber);
		Assert(result);
	}

	void PushJob(JobInstance&& job)
	{
		while (!g_jobPool.push_back(std::move(job))) { std::this_thread::yield(); }
	}

	void SwitchToFiber(const FiberDecl& fiber)
	{
		Assert(fiber.threadIndex == g_workerID);
		g_currentFiber = fiber;
		Fiber::SwitchTo(fiber.pFiber);
	}

	void WaitForCounter_Fiber(CounterInstance* pCounter)
	{
		Worker& worker = *g_pWorkers[g_workerID];
		while (pCounter->GetValue() > 0)
		{
			FiberDecl next;
			const bool isSleeping = worker.sleepingFibers.pop_front(next);
			if (!isSleeping && !worker.freeFibers.pop_front(next) && !worker.CreateFiber(g_workerID, next))
			{
				Assert(false);
				std::this_thread::yield();
				continue;
			}

			WaitingFiberEntry entry{ .m_fiber = g_currentFiber, .m_pNext = nullptr };
			if (pCounter->TryAddWaitingFiber(entry))
			{
				SwitchToFiber(next);
				return;
			}

			bool result = isSleeping ? worker.sleepingFibers.push_back(next) : worker.freeFibers.push_back(next);
			Assert(result);
			return;
		}
	}

	void WorkerMainLoop(void*)
	{
		Worker& worker = *g_pWorkers[g_workerID];
		{
			JobInstance job;
			while (g_workerRunning.load())
			{
				FiberDecl next;
				if (worker.sleepingFibers.pop_front(next))
				{
					bool result = worker.freeFibers.push_back(g_currentFiber);
					Assert(result);
					SwitchToFiber(next);
					continue;
				}

				if (!g_jobPool.pop_front(job))
				{
					std::this_thread::yield();
					continue;
				}

				job.m_executable();
				job.m_executable = nullptr;

				job.m_pCounter->Decrement();
				job.m_pCounter->Release();

				g_finishedLabel.fetch_add(1);
			}
		}

		Fiber::SwitchTo(g_pMainFiber);
	}

	void InitWorker(dU32 workerID)
	{
		Assert(!g_pMainFiber);
		g_pMainFiber = Fiber::ConvertThreadToFiber();
		g_workerID = workerID;

		Worker& worker = *g_pWorkers[g_workerID];
		for (dU32 i = 0; i < g_initialFiberPerThread; i++)
		{
			FiberDecl fiber;
			worker.CreateFiber(workerID, fiber);
			bool result = worker.freeFibers.push_back(fiber);
			Assert(result);
		}

		FiberDecl first;
		bool result = worker.freeFibers.pop_front(first);
		Assert(result);
		SwitchToFiber(first);

		g_currentFiber = { nullptr, 0 };
		for (dU32 i = 0; i < worker.fiberCount; i++)
			Fiber::Delete(worker.fibers[i]);
		Fiber::ConvertFiberToThread();
		g_pMainFiber = nullptr;
	}

	void Initialize(dU32 workerCount)
	{
		g_workerRunning = true;
		g_pWorkers.reserve(workerCount);
		for (dU32 workerID = 0; workerID < workerCount; ++workerID)
			g_pWorkers.push_back(new Worker(&InitWorker));
		for (dU32 workerID = 0; workerID < workerCount; ++workerID)
			g_pWorkers[workerID]->Run(workerID);
	}

	void Shutdown()
	{
		Wait();
		g_workerRunning = false;

		for (Worker* pWorker : g_pWorkers)
		{
			if (pWorker->thread.joinable())
				pWorker->thread.join();
		}

		while (!g_pWorkers.empty())
		{
			Worker* pWorker{ g_pWorkers.back() };
			g_pWorkers.pop_back();
			delete(pWorker);
		}
	}

	void Wait()
	{
		Assert(!g_currentFiber.pFiber);
		while (g_finishedLabel.load() < g_currentLabel.load()) { std::this_thread::yield(); }
	}

	void WaitForCounter(const Counter& counter)
	{
		CounterInstance* pCounter = counter.m_pCounterInstance;
		if (!pCounter)
			return;

		if (g_currentFiber.pFiber)
			WaitForCounter_Fiber(pCounter);
		else
			while (pCounter->GetValue() != 0) { std::this_thread::yield(); }
	}

	dU32 GetWorkerID()
	{
		return g_workerID;
	}

	dU32 GetWorkerCount()
	{
		return (dU32)g_pWorkers.size();
	}

	void SpinLock::lock()
	{
		while (true)
		{
			while (m_lock.load(std::memory_order_relaxed))
			{
				_mm_pause();
			}

			if (!m_lock.exchange(true, std::memory_order_acquire))
				break;
		}
	}

	void SpinLock::unlock()
	{
		m_lock.store(false, std::memory_order_release);
	}

	Counter::Counter()
	{
	}

	Counter::Counter(const Counter& other)
	{
		if (other.m_pCounterInstance)
		{
			m_pCounterInstance = new CounterInstance();
			other.m_pCounterInstance->AddListener(*m_pCounterInstance);
		}
	}

	Counter& Counter::operator=(const Counter& other)
	{
		if (other.m_pCounterInstance == m_pCounterInstance)
			return *this;
		Reset();
		if (other.m_pCounterInstance)
		{
			m_pCounterInstance = new CounterInstance();
			other.m_pCounterInstance->AddListener(*m_pCounterInstance);
		}
		return *this;
	}

	Counter::Counter(Counter&& other)
	{
		m_pCounterInstance = other.m_pCounterInstance;
		other.m_pCounterInstance = nullptr;
	}

	Counter& Counter::operator=(Counter&& other)
	{
		if (other.m_pCounterInstance == m_pCounterInstance)
			return *this;
		Reset();
		m_pCounterInstance = other.m_pCounterInstance;
		other.m_pCounterInstance = nullptr;
		return *this;
	}

	Counter::~Counter()
	{
		Reset();
	}

	Counter& Counter::operator++()
	{
		if (!m_pCounterInstance)
			m_pCounterInstance = new CounterInstance();
		m_pCounterInstance->m_counter.fetch_add(1);
		return *this;
	}

	Counter& Counter::operator++(int)
	{
		return ++(*this);
	}

	Counter& Counter::operator--()
	{
		Assert(m_pCounterInstance);
		m_pCounterInstance->Decrement();
		return *this;
	}

	Counter& Counter::operator--(int)
	{
		return --(*this);
	}

	Counter& Counter::operator+=(const Counter& other)
	{
		if (other.m_pCounterInstance == m_pCounterInstance)
			return *this;
		CounterInstance* pNewInstance = new CounterInstance();
		if (m_pCounterInstance)
		{
			m_pCounterInstance->AddListener(*pNewInstance);
			m_pCounterInstance->Release();
		}
		if (other.m_pCounterInstance)
			other.m_pCounterInstance->AddListener(*pNewInstance);
		m_pCounterInstance = pNewInstance;
		return *this;
	}

	Counter Counter::operator+(const Counter& other)
	{
		Counter counter{ other };
		if (m_pCounterInstance)
			counter += *this;
		return counter;
	}

	dU32 Counter::GetValue() const
	{
		return m_pCounterInstance ? m_pCounterInstance->GetValue() : 0;
	}

	void Counter::Reset()
	{
		if (m_pCounterInstance)
			m_pCounterInstance->Release();
		m_pCounterInstance = nullptr;
	}

	void JobBuilder::DispatchExplicitFence()
	{
		if (m_accumulateCounter.GetValue() > 0)
		{
			m_waitCounter = m_accumulateCounter;
			m_accumulateCounter = Counter();
		}
	}

	void JobBuilder::DispatchWait(const Counter& counter)
	{
		m_waitCounter += counter;
	}

	const Counter& JobBuilder::ExtractWaitCounter()
	{
		DispatchExplicitFence();
		return m_waitCounter;
	}

	void JobBuilder::DispatchJobInternal(const std::function<void()>& job)
	{
		m_accumulateCounter++;
		g_currentLabel.fetch_add(1);
		m_accumulateCounter.m_pCounterInstance->m_refCount.fetch_add(1);

		JobInstance instance{ job, m_accumulateCounter.m_pCounterInstance };
		CounterInstance* pFence = m_waitCounter.m_pCounterInstance;
		if (pFence && pFence->GetValue() > 0)
		{
			PendingJob* pPendingJob = new PendingJob{ std::move(instance), nullptr };
			if (pFence->TryAddPendingJob(*pPendingJob))
				return;
			instance = std::move(pPendingJob->m_job);
			delete pPendingJob;
		}
		PushJob(std::move(instance));
	}
}
