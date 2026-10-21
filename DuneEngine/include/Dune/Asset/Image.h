#pragma once

#include "Dune/Utilities/Handle.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/Handles.h"

namespace Dune
{
	struct Image
	{
		FileSystem::SerializationID<EFileType::Image> source;
		bool                                          isSRGB{ false };
		Graphics::TextureHandle                       texture;
	};

	using ImageHandle = Handle<Image>;
}
