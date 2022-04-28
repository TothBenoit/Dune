#pragma once

#include <cstdint>

namespace Dune
{

	struct VertexData
	{
		dVector3 position;
		dVector4 color;
	};

	struct GraphicsElementData
	{
		const void* vertices;
		const void* indices;

		uint32_t m_verticesCount;
		uint32_t m_indexCount;
	};

	class GraphicsElement
	{
	public:
		GraphicsElement(const GraphicsElementData data);
		virtual ~GraphicsElement() = default;

	protected:
		virtual void UploadVertexBuffer() = 0;
		virtual void UploadIndexBuffer() = 0;

	protected:
		GraphicsElementData m_data;
	};
}

