#pragma once

namespace Dune
{
	class SceneGraph
	{
	public:
		struct Node
		{
			[[nodiscard]] inline EntityID GetSelf() const { return m_self; }
			[[nodiscard]] inline const Node* GetParent() const { return m_parent; }
			[[nodiscard]] inline const dString& GetName() const { return m_name; }
			[[nodiscard]] inline size_t GetChildCount() const { return m_children.size(); }
			[[nodiscard]] inline const dList<Node*>& GetChildren() const { return m_children; }

		private:
			friend class SceneGraph;
			EntityID m_self;
			Node* m_parent = nullptr;
			dList<Node*> m_children;
			dString m_name;
		};

		SceneGraph();
		~SceneGraph() = default;
		DISABLE_COPY_AND_MOVE(SceneGraph);

		void AddNode(EntityID id, const dString& name = "New Node", EntityID m_parent = ID::invalidID);
		void DeleteNode(EntityID id);
		bool HasNode(EntityID id) const;
		const Node* GetNode(EntityID id) const;
		const Node* GetRoot() const { return m_root; }

	private:
		Node* m_root;
		dHashMap<EntityID, Node> m_lookup;
	};
}