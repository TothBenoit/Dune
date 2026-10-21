#pragma once

#include "Dune/Assets/Image.h"
#include "Dune/Core/RenderTypes.h"
#include "Dune/Platform/RHI/PipelineState.h"

namespace Dune
{
	inline constexpr dU32 kInvalidModelIndex{ dU32(-1) };

	struct ModelImage
	{
		FileSystem::SerializationID<EFileType::Image> source;
	};

	struct ModelMaterial
	{
		dVec3             baseColor{ 1.0f, 1.0f, 1.0f };
		float             metalness{ 0.0f };
		float             roughness{ 0.5f };
		float             alphaCutoff{ 0.5f };
		EAlphaMode        alphaMode{ EAlphaMode::Opaque };
		RHI::ECullingMode faceCulling{ RHI::ECullingMode::Back };
		dU32              albedoImage{ kInvalidModelIndex };
		dU32              normalImage{ kInvalidModelIndex };
		dU32              roughnessMetalnessImage{ kInvalidModelIndex };
	};

	struct ModelMesh
	{
		dVector<Vertex>  vertices;
		dVector<dU32>    indices;
		dVector<SubMesh> subMeshes;
		dU32             materialSlotCount{ 0 };
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
		dVector<ModelMesh>     meshes;
		dVector<ModelNode>     nodes;
	};

	struct Model
	{
		struct Node
		{
			dVec3                       position{ 0.0f, 0.0f, 0.0f };
			dVec4                       rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
			Graphics::MeshHandle        mesh;
			Graphics::MaterialSetHandle materials;
		};

		FileSystem::SerializationID<EFileType::Model> source;
		dVector<ImageHandle>                          images;
		dVector<Graphics::MaterialHandle>             materials;
		dVector<Graphics::MeshHandle>                 meshes;
		dVector<Node>                                 nodes;
	};

	using ModelHandle = Handle<Model>;
}
