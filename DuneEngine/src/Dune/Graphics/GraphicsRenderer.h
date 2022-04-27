#pragma once

#include <memory.h>

namespace Dune
{
	class GraphicsRenderer
	{
	public:
		virtual ~GraphicsRenderer() = default;
		GraphicsRenderer(const GraphicsRenderer&) = delete;
		GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;

		static std::unique_ptr<GraphicsRenderer> Create(const void * window);

		virtual void WaitForGPU() = 0;
		virtual void Render() = 0;
		virtual void Present() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;

	protected:
		GraphicsRenderer() = default;
	};
}

