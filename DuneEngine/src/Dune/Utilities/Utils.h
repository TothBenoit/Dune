#pragma once

namespace Dune::Utils
{
	dU32 AlignTo(dU32 size, dU32 alignment)
	{
		Assert(alignment > 0);
		return ((size + alignment - 1) / alignment) * alignment;
	}

	dU64 AlignTo(dU64 size, dU64 alignment)
	{
		Assert(alignment > 0);
		return ((size + alignment - 1) / alignment) * alignment;
	}
}