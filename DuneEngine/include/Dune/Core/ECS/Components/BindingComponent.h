#pragma once

namespace Dune
{
	struct BindingComponent
	{
		EntityID parent{ ID::invalidID };
		dVector<EntityID> children;
	};
}