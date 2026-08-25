#pragma once

#include "Dune/Graphics/RHI/Barrier.h"

namespace Dune
{
	namespace Graphics
	{
		class Renderer;
		struct RenderPassContext;

		enum class EResourceTag : dU32
		{
			DepthBuffer,
			ShadowMaps,
			LightBuffer,
			LightMatrices,
			HDRTarget,
			OutputTarget,
			Count,
		};

		struct ResourceAccess
		{
			EResourceTag id;
			EResourceState state;
		};

		struct RenderPassDesc
		{
#if _DEBUG
			const char* name;
#endif
			dVector<ResourceAccess> reads;
			dVector<ResourceAccess> writes;
		};

		struct RenderPass
		{
			void (*pExecute)(RenderPassContext&, void*);
			void (*pShutdown)(Renderer&, void*);
			void* pData;
			RenderPassDesc desc;
		};
	}
}
	

