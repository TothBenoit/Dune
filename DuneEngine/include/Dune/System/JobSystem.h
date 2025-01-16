#pragma once

namespace Dune::Job
{
    class CounterInstance;
    class Counter;

    void Initialize();
    void Shutdown();
    void Wait();
    void WaitForCounter(const Counter& counter);
    dU32 GetWorkerID();
    dU32 GetWorkerCount();

    class SpinLock
    {
    public:
        void lock();
        void unlock();

    private:
        std::atomic<bool> m_lock{ false };
    };

    class Counter
    {
    public:
        Counter();

        Counter& operator++();
        Counter& operator++(int);
        Counter& operator--();
        Counter& operator--(int);
        Counter& operator+=(const Counter& other);

        uint64_t GetValue() const;

    private:
        CounterInstance* GetPtrValue() const;

    private:
        friend void WaitForCounter_Fiber(const Counter&);
        friend CounterInstance;

        std::shared_ptr<CounterInstance> m_pCounterInstance;
    };

    enum class Fence
    {
        None,
        With
    };

    class JobBuilder
    {
    public:
        JobBuilder();

        template<Fence fenceType = Fence::With>
        void DispatchJob(const std::function<void()>& job);
        void DispatchExplicitFence();
        void DispatchWait(const Counter& counter);
        const Counter& ExtractWaitCounter();

    private:
        void DispatchJobInternal(const std::function<void()>& job);

    private:     
        Counter     m_counter;
        Counter     m_fence;
    };

    template<Fence fenceType>
    void JobBuilder::DispatchJob(const std::function<void()>& job)
    {
        DispatchJobInternal(job);
        if constexpr (fenceType == Fence::With)
        {
            DispatchExplicitFence();
        }
    }
}