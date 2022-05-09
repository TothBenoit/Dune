#pragma once

#include "Dune/Core/ECS/Entity.h"

namespace Dune
{
	struct SceneGraphNode
	{
		EntityID m_parent;
		EntityID m_self;
		dVector<EntityID> m_children;
	};

	class SceneGraph
	{
	public:
		SceneGraph();

	private:

		dVector<SceneGraphNode> m_nodes;
	};
}