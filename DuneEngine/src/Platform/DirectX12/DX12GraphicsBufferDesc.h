#pragma once
#include "Dune/Graphics/GraphicsBufferDesc.h"

namespace Dune
{
	class DX12GraphicsBufferDesc : public GraphicsBufferDesc
	{
		~DX12GraphicsBufferDesc() override;
	};
}

