#include "pch.h"
#include "Dune/Utilities/StringUtils.h"
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
}
