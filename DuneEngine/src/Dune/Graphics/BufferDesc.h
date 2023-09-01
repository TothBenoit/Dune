#pragma once

namespace Dune
{
	enum class EBufferUsage
	{
		Default,
		Upload,
	};

	struct BufferDesc
	{
		const wchar_t* debugName = nullptr;

		dU32 byteSize = 0;
		EBufferUsage usage = EBufferUsage::Default;
		const void* pData = nullptr;
	};
}