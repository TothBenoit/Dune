#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune
{
	class Scene;

	namespace Graphics
	{
		class Renderer;
		struct RenderPassContext;

		struct ShadowData
		{
			RootSignature shadowRS;
			PipelineState shadowPSO;
		};

		class Shadow
		{
		public:
			static RenderPassDesc GetDesc();
			static ShadowData* Create(Renderer& renderer);
			static void        Execute(RenderPassContext& context, ShadowData* pData);
			static void        Destroy(Renderer& renderer, ShadowData* pData);
		private:
			Shadow() = delete;
		};
	}
}
