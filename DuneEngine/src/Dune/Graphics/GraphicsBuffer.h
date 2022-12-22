#pragma once

#include "Dune/Graphics/GraphicsBufferDesc.h"

namespace Dune
{
	class GraphicsBuffer
	{
	public:
		virtual ~GraphicsBuffer() = default;

		inline dU32 GetSize() const { return m_size; };
		inline EBufferUsage GetUsage() const { return m_usage; };

	protected:
		GraphicsBuffer() = default;

	protected:
		dU32 m_size;
		EBufferUsage m_usage;
	};
}