#pragma once

#include "GraphicsDevice.h"

namespace Dune
{
	class GraphicsCore
	{
	public:
		GraphicsCore() = delete;
		~GraphicsCore() = delete;

		static void Init();
		static void Shutdown();
		static void Render();

	private:
		static std::unique_ptr<GraphicsDevice> m_GraphicsDevice;
	};
}

