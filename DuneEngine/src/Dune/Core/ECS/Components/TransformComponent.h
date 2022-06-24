#pragma once

namespace Dune
{
	// TODO : find a consistent way to notify when a transform has been modified. Should we use only function like unity ? Or hide transform component and use an interface ?
	struct TransformComponent
	{
		dVec3 position = { 0.f,0.f,0.f };
		dVec3 rotation = { 0.f, 0.f, 0.f };
		dVec3 scale = { 1.f,1.f,1.f };
		
		dMatrix matrix;
	};
}