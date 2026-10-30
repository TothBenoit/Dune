#pragma once

#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	struct RendererDesc
	{
		void* (*pCreate)(Renderer&){ nullptr };
		void (*pSetup)(RenderPassContext&, void*){ nullptr };
		void (*pDestroy)(Renderer&, void*){ nullptr };
	};
}
