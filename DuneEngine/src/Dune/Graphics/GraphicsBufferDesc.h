#pragma once

namespace Dune
{
	struct GraphicsBufferDesc
	{
		virtual ~GraphicsBufferDesc() = default;

		uint32_t size;
	};
}