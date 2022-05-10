#pragma once

namespace Dune
{
	struct SceneGraphNode
	{
		inline EntityID GetSelf() const { return m_self; }
		inline const SceneGraphNode* GetParent() const { return m_parent; }
		inline const dString& GetName() const { return m_name; }
		inline size_t GetChildCount() const { return m_children.size(); }
		inline const dList<SceneGraphNode*>& GetChildren() const { return m_children; }

	private:
		friend class SceneGraph;
		EntityID m_self;
		SceneGraphNode* m_parent = nullptr;
		dList<SceneGraphNode*> m_children;
		dString m_name;
	};

	class SceneGraph
	{
	public:
		SceneGraph();
		void AddNode(EntityID id, const dString& name = "New Node", EntityID m_parent = ID::invalidID);
		void DeleteNode(EntityID id);
		bool HasNode(EntityID id);
		void Draw();

	private:
		SceneGraphNode* m_root;
		dHashMap<EntityID, SceneGraphNode> m_lookup;

	};
}