#pragma once

namespace Dune
{
	struct Transform
	{
		static constexpr bool kTrackChanges = true;

		dVec3 position;
		dQuat rotation;
		float scale;
	};
}
