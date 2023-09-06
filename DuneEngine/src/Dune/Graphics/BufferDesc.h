#pragma once

namespace Dune
{
	enum class EBufferUsage
	{
		Vertex,
		Index,
		Constant,
		Structured,
	};

	enum class EBufferMemory
	{
		CPU,
		GPU
	};


	struct BufferDesc
	{
		const wchar_t* debugName = nullptr;

		dU32 byteSize = 0;
		EBufferUsage usage = EBufferUsage::Constant;
		EBufferMemory memory = EBufferMemory::CPU;
		const void* pData = nullptr;
	};
}