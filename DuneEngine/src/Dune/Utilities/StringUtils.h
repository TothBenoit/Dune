#pragma once

namespace Dune
{
	namespace dStringUtils
	{
		[[nodiscard]] inline const dString printf(const char* fmt, ...)
		{
			char tempStr[4096];

			va_list args;
			va_start(args, fmt);
			_vsnprintf_s(tempStr, 4096, fmt, args);
			va_end(args);

			return dString(tempStr);
		}

		[[nodiscard]] inline const dWString wprintf(const wchar_t* fmt, ...)
		{
			wchar_t tempStr[4096];

			va_list args;
			va_start(args, fmt);
			_vsnwprintf_s(tempStr, 4096, fmt, args);
			va_end(args);

			return dWString(tempStr);
		}
	}
}
