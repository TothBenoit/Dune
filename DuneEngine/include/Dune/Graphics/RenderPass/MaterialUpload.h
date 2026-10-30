#pragma once

#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct MaterialUploadData
	{
		RGBuffer handle;
	};

	class MaterialUpload
	{
	public:
		static MaterialUploadData* Create(Renderer& renderer);
		static void                Setup(RenderPassContext& context, MaterialUploadData* pData);
		static void                Destroy(Renderer& renderer, MaterialUploadData* pData);
	private:
		MaterialUpload() = delete;
	};
}
