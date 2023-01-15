#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Common/Handle.h"

namespace Dune
{
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
		GraphicsElement(const std::shared_ptr<Mesh> mesh, const InstanceData& instanceData);
		~GraphicsElement();
		
		DISABLE_COPY(GraphicsElement);

		GraphicsElement(GraphicsElement&& other);
		GraphicsElement& operator=(GraphicsElement&& other);

		inline const Mesh* GetMesh() const { return m_mesh.get(); };
		Handle<Buffer> GetInstanceData() const { return m_instanceData; }
		void UpdateInstanceData(const InstanceData& data);

	private:
		//TODO: use handle instead of shared_ptr
		std::shared_ptr<Mesh> m_mesh;
		Handle<Buffer> m_instanceData;
	};
}

