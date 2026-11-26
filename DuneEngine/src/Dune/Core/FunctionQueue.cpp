#include "pch.h"
#include "Dune/Core/FunctionQueue.h"

namespace Dune
{
	void FunctionQueue::Execute()
	{
		for (const Entry& entry : m_entries)
			entry.pExecute(entry.pFunction);
		m_entries.clear();
	}
}
