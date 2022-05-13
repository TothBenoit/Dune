#pragma once

namespace Dune
{
	struct BindingComponent
	{
		EntityID parent;
		dVector<EntityID> children;
	};
}