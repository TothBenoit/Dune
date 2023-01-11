#include "pch.h"
#include "GraphicsElement.h"
#include "Dune/Core/EngineCore.h"
#include "Dune/Graphics/GraphicsBuffer.h"

namespace Dune
{
	GraphicsElement::GraphicsElement(const std::shared_ptr<Mesh> mesh, const InstanceData& instanceData)
		: m_mesh{mesh}
	{
		GraphicsBufferDesc desc{ EBufferUsage::Upload };
		m_instanceData = EngineCore::GetGraphicsRenderer().CreateBuffer(desc, &instanceData, InstanceDataSize);
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
		EngineCore::GetGraphicsRenderer().UpdateBuffer(m_instanceData.get(), &data, InstanceDataSize);
	}
}