#pragma once

#include "Dune/Core/Handle.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/Handles.h"

namespace Dune
{
	struct Image
	{
		FileSystem::SerializationID<EFileType::Image> source;
		Graphics::TextureHandle                       texture;
	};

	using ImageHandle = Handle<Image>;
}
