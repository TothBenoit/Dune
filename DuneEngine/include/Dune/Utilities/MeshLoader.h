#pragma once

#include "Dune/Common/Handle.h"

namespace Dune::Graphics
{
	class Mesh;
	struct Device;
	// TODO : Create custom mesh format to load. 
	namespace MeshLoader
	{
		dVector<Handle<Mesh>> Load(Device* pDevice, const char* path);
	}
}