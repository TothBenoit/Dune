#pragma once

#include "Dune/Graphics/RHI/TextureData.h"
#include "Dune/Graphics/MeshData.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	inline constexpr dU32 kInvalidModelIndex{ dU32(-1) };

	struct ModelImage
	{
		TextureData data;
		bool      isSRGB{ false };
	};

	struct ModelMaterial
	{
		MaterialDesc desc;
		dU32         albedoImage{ kInvalidModelIndex };
		dU32         normalImage{ kInvalidModelIndex };
		dU32         roughnessMetalnessImage{ kInvalidModelIndex };
	};

	struct ModelNode
	{
		dVec3         position{ 0.0f, 0.0f, 0.0f };
		dVec4         rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
		dU32          mesh{ kInvalidModelIndex };
		dVector<dU32> materials;
	};

	struct ModelData
	{
		dVector<ModelImage>    images;
		dVector<ModelMaterial> materials;
		dVector<MeshData>      meshes;
		dVector<ModelNode>     nodes;
	};
}
