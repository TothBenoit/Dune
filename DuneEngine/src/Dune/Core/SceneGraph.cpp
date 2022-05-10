#include "pch.h"
#include "SceneGraph.h"
#include "Dune/Core/Logger.h"

namespace Dune
{
	SceneGraph::SceneGraph()
	{
		m_lookup.reserve(ID::GetMaximumIndex());

		m_root.m_parent = nullptr;
		m_root.m_self = ID::invalidID;
		m_root.m_name = "Root";
	}
	void SceneGraph::AddNode(EntityID id, const dString& name, SceneGraphNode* parent)
	{
		if (m_lookup.find(id) != m_lookup.end())
		{
			LOG_CRITICAL("Node already present");
			return;
		}

		SceneGraphNode& node = m_lookup[id];
		node.m_self = id;
		node.m_name = name;

		if (parent == nullptr)
		{
			parent = &m_root;
		}
		else
		{
			parent->m_children.push_back(&node);
		}

		node.m_parent = parent;

	}
	void SceneGraph::DeleteNode(EntityID id)
	{
		if (m_lookup.find(id) == m_lookup.end())
		{
			LOG_CRITICAL("Node doesn't exist");
			return;
		}

		SceneGraphNode& node = m_lookup[id];
		for (SceneGraphNode* child : node.m_children)
		{
			child->m_parent = &m_root;
		}

		node.m_parent->m_children.remove(&node);
		m_lookup.erase(id);
	}
}
