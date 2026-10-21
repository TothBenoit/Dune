#pragma once

namespace Dune::Fiber
{
	using FiberEntry = void (*)(void* pData);

	[[nodiscard]] void* ConvertThreadToFiber();
	void ConvertFiberToThread();
	[[nodiscard]] void* Create(dSizeT stackCommitSize, dSizeT stackReserveSize, FiberEntry pEntry, void* pData);
	void SwitchTo(void* pFiber);
	void Delete(void* pFiber);
}
