#pragma once

namespace Dune
{
	namespace dStringUtils
	{
		inline const dString printf(const char* fmt, ...)
		{
			char tempStr[4096];

			va_list args;
			va_start(args, fmt);
			_vsnprintf_s(tempStr, 4096, fmt, args);
			va_end(args);

			return dString(tempStr);
		}
	}
}
