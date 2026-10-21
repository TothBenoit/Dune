#include "pch.h"
#include "Dune/Platform/Fiber.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Dune::Fiber
{
	void* ConvertThreadToFiber()
	{
		return ::ConvertThreadToFiber(nullptr);
	}

	void ConvertFiberToThread()
	{
		::ConvertFiberToThread();
	}

	void* Create(dSizeT stackCommitSize, dSizeT stackReserveSize, FiberEntry pEntry, void* pData)
	{
		return ::CreateFiberEx(stackCommitSize, stackReserveSize, 0, pEntry, pData);
	}

	void SwitchTo(void* pFiber)
	{
		::SwitchToFiber(pFiber);
	}

	void Delete(void* pFiber)
	{
		::DeleteFiber(pFiber);
	}
}
