#pragma once

namespace Dune
{
	class NonCopyable
	{
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	};
}


