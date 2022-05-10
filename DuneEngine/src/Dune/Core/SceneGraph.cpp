#include "pch.h"
#include "SceneGraph.h"
#include "Dune/Core/Logger.h"

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
			LOG_CRITICAL("Node already present");
			return;
		}

		SceneGraphNode& node = m_lookup[id];
		node.m_self = id;
		node.m_name = name;

		if (!HasNode(parent))
		{
			LOG_CRITICAL("Parent hasn't been added");
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
			LOG_CRITICAL("Node doesn't exist");
			return;
		}

		SceneGraphNode& node = m_lookup[id];
		for (SceneGraphNode* child : node.m_children)
		{
			child->m_parent = m_root;
		}

		node.m_parent->m_children.remove(&node);
		m_lookup.erase(id);
	}

	bool SceneGraph::HasNode(EntityID id)
	{
		return m_lookup.find(id) != m_lookup.end();
	}

	void DrawNode(const SceneGraphNode* node, size_t depth)
	{
		for (const SceneGraphNode* child : node->GetChildren())
		{
			DrawNode(child, depth++);
		}
		std::cout << "NodeName: " << node->GetName() << " NodeID: " << node->GetSelf() << " Depth: " << depth << "\n";

	}

	void SceneGraph::Draw()
	{
		for (const SceneGraphNode* child : m_root->GetChildren())
		{
			DrawNode(child, 0);
		}
	}
}
