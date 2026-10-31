#pragma once

#include "Dune/Core/Handle.h"

namespace Dune::Graphics
{
	class Mesh;
	class Texture;
	struct Material;
	struct MaterialSet;
	class RenderScene;
	struct RenderObject;
	struct RenderLight;
}

namespace Dune
{
	using MeshHandle = Handle<Graphics::Mesh>;
	using TextureHandle = Handle<Graphics::Texture>;
	using MaterialHandle = Handle<Graphics::Material>;
	using MaterialSetHandle = Handle<Graphics::MaterialSet>;
	using RenderSceneHandle = Handle<Graphics::RenderScene>;
	using RenderObjectHandle = Handle<Graphics::RenderObject>;
	using RenderLightHandle = Handle<Graphics::RenderLight>;
}
