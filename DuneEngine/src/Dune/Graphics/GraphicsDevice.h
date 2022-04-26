#pragma once

#include <memory>

namespace Dune
{
	class GraphicsDevice
	{
	public:
		virtual ~GraphicsDevice() = default;
		static std::unique_ptr<GraphicsDevice> Create();
	};
}

