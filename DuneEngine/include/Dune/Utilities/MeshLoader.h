#pragma once

namespace Dune::Graphics
{
	class Mesh;
	class Device;
	// TODO : Create custom mesh format to load. 
	namespace MeshLoader
	{
		dVector<Mesh*> Load(Device* pDevice, const char* path);
	}
}