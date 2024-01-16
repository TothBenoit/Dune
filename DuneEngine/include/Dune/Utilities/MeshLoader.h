#pragma once

#include "Dune/Common/Handle.h"

namespace Dune::Graphics
{
	class Mesh;
	class View;
	//TODO : Move to Engine
	namespace MeshLoader
	{
		dVector<Handle<Mesh>> Load(View* pView, const dString& path);
	}
}