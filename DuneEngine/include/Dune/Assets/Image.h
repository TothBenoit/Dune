#pragma once

#include "Dune/Core/Handle.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Platform/RHI/Format.h"

namespace Dune
{
	struct ImageData
	{
		RHI::EFormat format{ RHI::EFormat::Unknown };
		dU32         width{ 0 };
		dU32         height{ 0 };
		dU32         mipLevels{ 1 };
		dVector<dU8> bytes;
	};

	struct Image
	{
		FileSystem::SerializationID<EFileType::Image> source;
		RHI::EFormat                                  format{ RHI::EFormat::Unknown };
		dU32                                          width{ 0 };
		dU32                                          height{ 0 };
		dU32                                          mipLevels{ 1 };
	};

	using ImageHandle = Handle<Image>;
}
