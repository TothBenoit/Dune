#pragma once

#include "Dune/Common/Types.h"
#include "Dune/Graphics/GraphicsCore.h"
#include <vector>

namespace Dune
{
	struct Vertex
	{
		dVector3 position;
		dVector4 color;
	};

	class Mesh
	{
	public:
		Mesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);

		void UploadBuffer();
		bool IsUploaded() const { return m_isUploaded; }

	private:
		std::vector<uint32_t> m_indices;
		std::vector<Vertex> m_vertices;

		bool m_isUploaded = false;

		std::unique_ptr<GraphicsBuffer> m_indexBuffer = nullptr;
		std::unique_ptr<GraphicsBuffer> m_vertexBuffer = nullptr;
	};
}