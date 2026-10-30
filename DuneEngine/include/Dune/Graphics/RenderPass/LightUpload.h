#pragma once

#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct LightUploadData
	{
		RGBuffer handle;
	};

	class LightUpload
	{
	public:
		static LightUploadData* Create(Renderer& renderer);
		static void             Setup(RenderPassContext& context, LightUploadData* pData);
		static void             Destroy(Renderer& renderer, LightUploadData* pData);
	private:
		LightUpload() = delete;
	};
}
