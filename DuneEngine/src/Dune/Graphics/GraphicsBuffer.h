#pragma once

#include "Dune/Graphics/GraphicsBufferDesc.h"

namespace Dune
{
	class GraphicsBuffer
	{
	public:
		virtual ~GraphicsBuffer() = default;

		inline const GraphicsBufferDesc& GetDescription() const{ return m_desc; }


	private:
		GraphicsBufferDesc m_desc;
	};
}