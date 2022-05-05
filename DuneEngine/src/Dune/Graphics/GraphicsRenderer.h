#pragma once

#include "GraphicsElement.h"

namespace Dune
{
	class GraphicsBuffer;
	struct GraphicsBufferDesc;

	class GraphicsRenderer
	{
	public:
		virtual ~GraphicsRenderer() = default;
		GraphicsRenderer(const GraphicsRenderer&) = delete;
		GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;

		static std::unique_ptr<GraphicsRenderer> Create(const void * window);

		void AddGraphicsElement(const GraphicsElement& elem);

		virtual void WaitForGPU() = 0;
		virtual void Render() = 0;
		virtual void Present() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;
		virtual std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) = 0;

	protected:
		GraphicsRenderer() = default;
		std::vector<GraphicsElement> m_graphicsElements;
	};
}

