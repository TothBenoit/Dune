#include "pch.h"
#include "GraphicsElement.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"

namespace Dune
{
	GraphicsElement::GraphicsElement(const std::shared_ptr<Mesh> mesh, const InstanceData& instanceData)
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
		, m_instanceData {other.m_instanceData}
	{
		other.m_instanceData.Invalidate();
	}

	GraphicsElement& GraphicsElement::operator=(GraphicsElement && other)
	{
		m_mesh = std::move(other.m_mesh);
		m_instanceData = other.m_instanceData;
		other.m_instanceData.Invalidate();
		return *this;
	}

	void GraphicsElement::UpdateInstanceData(const InstanceData& data)
	{
		Renderer::GetInstance().UpdateBuffer(m_instanceData, &data, InstanceDataSize);
	}
}