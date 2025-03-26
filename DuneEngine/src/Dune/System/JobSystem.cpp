#include "pch.h"
#include "Dune/System/JobSystem.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Dune::Job
{
    class CounterInstance;

    void WorkerMainLoop();
    void UpdateWaitingJobs(const CounterInstance* counterInstance);
    void WaitForCounter_Fiber(const Counter& counter);
    void InitWorker(dU32 workerCount);

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
    class ThreadSafeRingBuffer
    {
    public:
        inline bool push_back(const T& item)
        {
            bool result = false;
            lock.lock();
            dSizeT next = (head + 1) % (capacity+1);
            if(next != tail)
            {
                data[head] = item;
                head = next;
                result = true;
            }
            lock.unlock();
            return result;
        }

        inline bool pop_front(T& item)
        {
            bool result = false;
            lock.lock();
            if (tail != head)
            {
                item = data[tail];
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
        Counter m_counter;
        Counter m_fence;
    };

    class CounterInstance
    {
    private:
        friend void UpdateWaitingJobs(const CounterInstance* counterInstance);
        friend void WorkerMainLoop(void * pData);
        friend Counter;

        uint32_t GetValue() const { return m_counter.load(); }

        void Decrement()
        {
            m_counter.fetch_sub(1);
            UpdateWaitingJobs(this);

            for (auto& waitingCounter : m_waitingCounters)
            {
                waitingCounter->Decrement();
            }
        }

        void Addlistener(Counter& counter) const
        {
            m_waitingCountersLock.lock();
            m_waitingCounters.push_back(counter.m_pCounterInstance);
            m_waitingCountersLock.unlock();
        }

    private:
        std::atomic<uint32_t> m_counter{ 0 };
        std::atomic<uint32_t> m_refCount{ 1 };
        mutable std::vector<CounterInstance*> m_waitingCounters;
        mutable SpinLock m_waitingCountersLock;
    };

	struct FiberDecl
	{
		void* pFiber;
		dU32 threadIndex;
	};

    const dU32 g_fiberPerThread{ 32 };
    struct Worker
    {
        Worker(void (*pFunc)(dU32))
            : pEntryPoint{ pFunc }
        {}
        
        void Run(dU32 threadID)
        {
            thread = std::thread(pEntryPoint, threadID);
        }

        void (*pEntryPoint)(dU32);
        std::thread thread;
        ThreadSafeRingBuffer<FiberDecl, g_fiberPerThread> freeFibers;
        ThreadSafeRingBuffer<FiberDecl, g_fiberPerThread> sleepingFibers;
    };

    std::vector<Worker*> g_pWorkers;
    ThreadSafeRingBuffer<JobInstance, 16384> g_jobPool;

    SpinLock g_waitingFibersLock;
    std::unordered_map <const CounterInstance*, std::vector<FiberDecl>> g_waitingFibers;

#pragma optimize( "", off )
    thread_local dU32 g_workerID;
	thread_local void* g_pMainFiber{ nullptr };
	thread_local FiberDecl g_pCurrentFiber{ nullptr, 0 };
#pragma optimize( "", on )

    std::atomic<uint64_t>   g_currentLabel{ 0 };
    std::atomic<uint64_t>   g_finishedLabel{ 0 };
    bool                    g_workerRunning{ false };

    dU32 GetWorkerID()
    {
        return g_workerID;
    }

    dU32 GetWorkerCount()
    {
        return (dU32)g_pWorkers.size();
    }

	void Switch_Fiber()
	{
		bool result = g_pWorkers[g_workerID]->freeFibers.push_back(g_pCurrentFiber);

		assert(result);
		if (!g_pWorkers[g_workerID]->sleepingFibers.pop_front(g_pCurrentFiber))
		{
			result = g_pWorkers[g_workerID]->freeFibers.pop_front(g_pCurrentFiber);
			assert(result);
		}
		assert(g_pCurrentFiber.pFiber);

		::SwitchToFiber(g_pCurrentFiber.pFiber);
	}

	void Switch()
	{
		if (g_pCurrentFiber.pFiber)
		{
			Switch_Fiber();
		}
		else
		{
			std::this_thread::yield();
		}
	}

    void UpdateWaitingJobs(const CounterInstance* counterInstance)
    {
        if (counterInstance->GetValue() != 0)
            return;

        g_waitingFibersLock.lock();
        auto it = g_waitingFibers.find(counterInstance);
        if (it != g_waitingFibers.end())
        {
            for (FiberDecl& fiber : it->second)
            {
                while (!g_pWorkers[fiber.threadIndex]->sleepingFibers.push_back(fiber)) { Switch_Fiber(); }
            }
            g_waitingFibers.erase(it);
        }
        g_waitingFibersLock.unlock();
    }

    void WorkerMainLoop(void* pData)
    {
        {
            JobInstance job;

            while (g_workerRunning)
            {
                if (g_jobPool.pop_front(job))
                {
                    if (job.m_fence.GetValue() > 0)
                    {
                        WaitForCounter_Fiber(job.m_fence);
                    }

                    {
                        (job.m_executable)();
                    }

                    Counter& counter = job.m_counter;
                    counter--;

                    g_finishedLabel.fetch_add(1);
                }

                Switch_Fiber();
            }
        }

        // Shutdown in progress
        // Every job must exit the previous scope to destroy the jobInstance

        if (!g_pWorkers[g_workerID]->sleepingFibers.pop_front(g_pCurrentFiber))
            if (!g_pWorkers[g_workerID]->freeFibers.pop_front(g_pCurrentFiber))
                g_pCurrentFiber.pFiber = g_pMainFiber;

        ::SwitchToFiber(g_pCurrentFiber.pFiber);
    }

    void InitWorker(dU32 workerID)
    {
        assert(!g_pMainFiber);
        g_pMainFiber = ::ConvertThreadToFiber(nullptr);
        g_workerID = workerID;

        for (dU32 i = 0; i < g_fiberPerThread; i++)
        {
            FiberDecl decl{ ::CreateFiber(64 * 1024, &WorkerMainLoop, nullptr), g_workerID };
            bool result = g_pWorkers[g_workerID]->freeFibers.push_back(decl);
            assert(result);
        }

        bool result = g_pWorkers[g_workerID]->freeFibers.pop_front(g_pCurrentFiber);
        assert(result);

        ::SwitchToFiber(g_pCurrentFiber.pFiber);
    }

    void Initialize()
    {
        g_workerRunning = true;

        dU32 numCores{ std::thread::hardware_concurrency() };

        dU32 workerCount = std::max(numCores - 3u, 1u);
        g_pWorkers.reserve(workerCount);
        for (dU32 workerID = 0; workerID < workerCount; ++workerID)
        {
            g_pWorkers.push_back(new Worker(&InitWorker));
            g_pWorkers.back()->Run(workerID);
        }
    }

    void Shutdown()
    {
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
        assert(!g_pCurrentFiber.pFiber); // Can't wait for all jobs to finish within a job
        while (g_finishedLabel.load() < g_currentLabel.load()) { }
    }

    void WaitForCounter(const Counter& counter)
    {
        if (g_pCurrentFiber.pFiber)
        {
            WaitForCounter_Fiber(counter);
        }
        else
        {
            while (counter.GetValue() != 0) { }
        }
    }

    void WaitForCounter_Fiber(const Counter& counter)
    {
		g_waitingFibersLock.lock();
		g_waitingFibers[counter.m_pCounterInstance].push_back(g_pCurrentFiber);
		g_waitingFibersLock.unlock();

        if (!g_pWorkers[g_workerID]->sleepingFibers.pop_front(g_pCurrentFiber))
		    while (!g_pWorkers[g_workerID]->freeFibers.pop_front(g_pCurrentFiber)) { Switch_Fiber(); }
		assert(g_pCurrentFiber.pFiber);

		::SwitchToFiber(g_pCurrentFiber.pFiber);
    }

    void SpinLock::lock()
    {
        while (g_workerRunning)
        {
            while (m_lock)
            {
                _mm_pause();
            }

            if (!m_lock.exchange(true))
                break;
        }
    }

    void SpinLock::unlock()
    {
        m_lock.store(false);
    }

    Counter::Counter()
    {
        m_pCounterInstance = new CounterInstance();
    }

    Counter::Counter(const Counter& other)
    {
        m_pCounterInstance = other.m_pCounterInstance;
        m_pCounterInstance->m_refCount.fetch_add(1);
    }

    Counter& Counter::operator=(const Counter& other)
    {
        if (m_pCounterInstance->m_refCount.fetch_sub(1) == 1)
            delete m_pCounterInstance;
        m_pCounterInstance = other.m_pCounterInstance;
        m_pCounterInstance->m_refCount.fetch_add(1);
        return *this;
    }

    Counter::Counter(Counter&& other)
    {
        m_pCounterInstance = other.m_pCounterInstance;
        m_pCounterInstance->m_refCount.fetch_add(1);
    }

    Counter& Counter::operator=(Counter&& other)
    {
        if (m_pCounterInstance->m_refCount.fetch_sub(1) == 1)
            delete m_pCounterInstance;
        m_pCounterInstance = other.m_pCounterInstance;
        m_pCounterInstance->m_refCount.fetch_add(1);
        return *this;
    }

    Counter::~Counter() 
    {
        if (m_pCounterInstance->m_refCount.fetch_sub(1) == 0)
            delete m_pCounterInstance;
    }

    Counter& Counter::operator++()
    {
        m_pCounterInstance->m_counter.fetch_add(1);
        return *this;
    }

    Counter& Counter::operator++(int)
    {
        return ++(*this);
    }

    Counter& Counter::operator--()
    {
        m_pCounterInstance->Decrement();
        return *this;
    }

    Counter& Counter::operator--(int)
    {
        return --(*this);
    }

    Counter& Counter::operator+=(const Counter& other)
    {
        other.m_pCounterInstance->Addlistener(*this);
        m_pCounterInstance->m_counter.fetch_add(other.GetValue());
        return *this;
    }

    uint32_t Counter::GetValue() const 
    { 
        return m_pCounterInstance->GetValue(); 
    }

    void JobBuilder::DispatchExplicitFence()
    {
        if (m_counter.GetValue() > 0)
        {
            m_fence = m_counter;
            m_counter = Counter();
        }
    }

    void JobBuilder::DispatchWait(const Counter& counter)
    {
        m_fence += counter;
    }

    const Counter& JobBuilder::ExtractWaitCounter()
    {
        DispatchExplicitFence();
        return m_fence;
    }

    void JobBuilder::DispatchJobInternal(const std::function<void()>& job)
    {
        m_counter++;
        g_currentLabel.fetch_add(1);

        while (!g_jobPool.push_back(JobInstance{ job, m_counter, m_fence })) { Switch(); }
    }
}