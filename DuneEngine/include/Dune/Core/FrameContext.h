#pragma once

namespace Dune
{
	inline constexpr dU32 kFrameRingSize = 4;

	struct FrameContext
	{
		dU64 frameNumber{ 0 };
		float deltaTime{ 0.0f };
	};
}
