#pragma once

namespace Dune
{
	struct SceneGraphNode
	{
		EntityID m_self;
		SceneGraphNode* m_parent = nullptr;
		dList<SceneGraphNode*> m_children;
		dString m_name;
	};

	class SceneGraph
	{
	public:
		SceneGraph();
		void AddNode(EntityID id, const dString& name = "New Node", SceneGraphNode * m_parent = nullptr);
		void DeleteNode(EntityID id);

	private:
		SceneGraphNode m_root;

		dHashMap<EntityID, SceneGraphNode> m_lookup;

	};
}