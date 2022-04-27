#pragma once

#include "Dune/Graphics/GraphicsRenderer.h"

namespace Dune
{
	class GraphicsCore
	{
	public:
		GraphicsCore() = delete;
		~GraphicsCore() = delete;

		static void Init(const void* window);
		static void Shutdown();
		static void Update();

	private:
		static std::unique_ptr<GraphicsRenderer> m_graphicsRenderer;
	};
}

