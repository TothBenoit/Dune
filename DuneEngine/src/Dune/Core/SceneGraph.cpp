#include "pch.h"
#include "SceneGraph.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"

namespace Dune
{
	SceneGraph::SceneGraph()
	{
		//m_lookup.reserve((size_t)(ID::GetMaximumIndex())+1);
		m_root = &m_lookup[ID::invalidID];

		m_root->m_parent = nullptr;
		m_root->m_self = ID::invalidID;
		m_root->m_name = "Root";
	}
	void SceneGraph::AddNode(EntityID id, const dString& name, EntityID parent)
	{
		if (HasNode(id))
		{
			LOG_CRITICAL("Tried to add Node already present");
			return;
		}

		Node& node = m_lookup[id];
		node.m_self = id;
		node.m_name = name;

		if (!HasNode(parent))
		{
			LOG_CRITICAL("Tried to set a non added Node as a parent");
			node.m_parent = m_root;
		}
		else
		{
			node.m_parent = &m_lookup[parent];
			node.m_parent->m_children.push_back(&node);
		}
	}

	void SceneGraph::DeleteNode(EntityID id)
	{
		if (!HasNode(id))
		{
			LOG_CRITICAL("Tried to delete a non added Node");
			return;
		}

		Node& node = m_lookup[id];
		for (Node* child : node.m_children)
		{
			child->m_parent = m_root;
		}

		node.m_parent->m_children.remove(&node);
		m_lookup.erase(id);
	}

	bool SceneGraph::HasNode(EntityID id) const
	{
		return m_lookup.find(id) != m_lookup.end();
	}
}
