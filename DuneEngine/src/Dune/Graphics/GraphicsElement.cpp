#include "pch.h"
#include "GraphicsElement.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"
#include "Dune/Graphics/Mesh.h"

namespace Dune
{
	GraphicsElement::GraphicsElement(Handle<Mesh> mesh, const InstanceData& instanceData)
		: m_mesh{mesh}
	{
		BufferDesc desc{ EBufferUsage::Upload };
		m_instanceData = Renderer::GetInstance().CreateBuffer(desc, &instanceData, InstanceDataSize);
	}

	GraphicsElement::~GraphicsElement()
	{
		if (m_instanceData.IsValid())
			Renderer::GetInstance().ReleaseBuffer(m_instanceData);
	}

	GraphicsElement::GraphicsElement(GraphicsElement&& other)
		: m_mesh{ std::move(other.m_mesh) }
		, m_instanceData {std::move(other.m_instanceData)}
	{}

	GraphicsElement& GraphicsElement::operator=(GraphicsElement && other)
	{
		m_mesh = std::move(other.m_mesh);
		m_instanceData = std::move(other.m_instanceData);
		return *this;
	}

	void GraphicsElement::UpdateInstanceData(const InstanceData& data)
	{
		Renderer::GetInstance().UpdateBuffer(m_instanceData, &data, InstanceDataSize);
	}
}