#include "pch.h"
#include "Windows.h"

namespace Dune::StringUtils
{
    dWString ToWide(const dString& str)
    {
        if (str.empty())
            return {};

        const dU32 byteSize = (dU32)str.size();
        const dU32 charCount = MultiByteToWideChar(CP_UTF8, 0, str.data(), byteSize, nullptr, 0);
        Assert(charCount > 0);

        dWString result(charCount, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.data(), byteSize, result.data(), charCount);
        return result;
    }

    dString ToNarrow(const dWString& str)
    {
        if (str.empty())
            return {};

        const dU32 charCount = (dU32)str.size();
        const dU32 byteSize = WideCharToMultiByte(CP_UTF8, 0, str.data(), charCount, nullptr, 0, nullptr, nullptr);
        Assert(byteSize > 0);

        dString result(byteSize, '\0');
        WideCharToMultiByte(CP_UTF8, 0, str.data(), charCount, result.data(), byteSize, nullptr, nullptr);
        return result;
    }
}
