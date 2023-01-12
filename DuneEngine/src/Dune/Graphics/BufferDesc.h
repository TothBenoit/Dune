#pragma once

namespace Dune
{
	enum class EBufferUsage
	{
		Default,
		Upload,
	};

	struct BufferDesc
	{
		EBufferUsage usage = EBufferUsage::Default;
	};
}