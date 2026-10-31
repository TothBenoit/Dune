#pragma once

namespace Dune
{
	struct FrameContext
	{
		dU64 frameNumber{ 0 };
		float deltaTime{ 0.0f };
	};
}
