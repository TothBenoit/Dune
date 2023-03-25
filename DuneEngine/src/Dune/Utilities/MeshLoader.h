#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Mesh;
	namespace MeshLoader
	{
		dVector<Handle<Mesh>> Load(const dString& path);
	}
}