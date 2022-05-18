#pragma once

namespace Dune
{
	enum class EBufferUsage
	{
		Default,
		Upload,
	};

	struct GraphicsBufferDesc
	{
		EBufferUsage usage = EBufferUsage::Default;
		dU32 size = 0;
	};
}