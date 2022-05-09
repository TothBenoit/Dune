#include "pch.h"
#include "SceneGraph.h"

namespace Dune
{
	SceneGraph::SceneGraph()
	{
		m_nodes.reserve(ID::GetMaximumIndex());
	}
}
