#pragma once

#include "Dune/Common/Handle.h"

namespace Dune::Graphics
{
	class Mesh;
	class View;
	// TODO : Create custom mesh format to load. 
	namespace MeshLoader
	{
		dVector<Handle<Mesh>> Load(View* pView, const char* path);
	}
}