#pragma once

#include "Dune/Asset/Image.h"

namespace Dune
{
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
