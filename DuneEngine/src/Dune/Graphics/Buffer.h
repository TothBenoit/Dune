#pragma once

#include "Dune/Graphics/BufferDesc.h"

namespace Dune
{
	class Buffer
	{
	public:
		virtual ~Buffer() = default;
		DISABLE_COPY_AND_MOVE(Buffer);

		inline dU32 GetSize() const { return m_size; };
		inline EBufferUsage GetUsage() const { return m_usage; };

	protected:
		Buffer() = default;

	protected:
		dU32 m_size;
		EBufferUsage m_usage;
	};
}