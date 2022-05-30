#pragma once

#include "Dune/Graphics/GraphicsBufferDesc.h"

namespace Dune
{
	class GraphicsBuffer
	{
	public:
		virtual ~GraphicsBuffer() = default;

		inline const GraphicsBufferDesc& GetDescription() const{ return m_desc; }
		inline void SetDescription(const GraphicsBufferDesc& desc) { m_desc = desc; }

	protected:
		GraphicsBuffer() = default;

	private:
		GraphicsBufferDesc m_desc;
	};
}