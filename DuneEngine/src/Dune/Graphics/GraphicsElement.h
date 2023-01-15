#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Mesh;
	class Buffer;

	// Change once per draw call
	struct InstanceData
	{
		dMatrix4x4	modelMatrix;
		dMatrix4x4	normalMatrix;
		dVec4		baseColor;
	};

	inline constexpr dU32 InstanceDataSize{ sizeof(InstanceData) };

	// Represent an element to draw
	class GraphicsElement
	{
	public:
		GraphicsElement(Handle<Mesh> mesh, const InstanceData& instanceData);
		~GraphicsElement();
		
		DISABLE_COPY(GraphicsElement);

		GraphicsElement(GraphicsElement&& other);
		GraphicsElement& operator=(GraphicsElement&& other);

		Handle<Mesh> GetMeshHandle() const { return m_mesh; };
		Handle<Buffer> GetInstanceData() const { return m_instanceData; }
		void UpdateInstanceData(const InstanceData& data);

	private:

		Handle<Mesh> m_mesh;
		Handle<Buffer> m_instanceData;
	};
}

