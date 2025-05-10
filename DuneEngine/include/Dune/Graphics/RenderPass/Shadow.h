#pragma once

#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"

namespace Dune
{
	struct Scene;

	namespace Graphics
	{
		class CommandList;
		class Shadow
		{
		public:
			void Initialize(Device* pDevice);
			void Destroy();

			void Render(Scene& scene, CommandList& commandList, const dMatrix4x4& viewProjection);

		private:
			RootSignature m_rootSignature;
			PipelineState m_pipeline;
			Device* m_pDevice{ nullptr };
		};
	}
}