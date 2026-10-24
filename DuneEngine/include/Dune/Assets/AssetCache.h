#pragma once

#include "Dune/Assets/Model.h"
#include "Dune/Assets/Image.h"
#include "Dune/Core/HandleAllocator.h"
#include "Dune/Core/ResourcePool.h"

namespace Dune
{
	class AssetCache
	{
	public:
		void Initialize();
		void Destroy();

		[[nodiscard]] ModelHandle AcquireModel(PathID<Model> source);
		[[nodiscard]] ModelHandle AddModel(Model&& model);
		[[nodiscard]] const Model& GetModel(ModelHandle handle) const;
		[[nodiscard]] bool ReleaseModel(ModelHandle handle, Model& outModel);

		[[nodiscard]] ImageHandle AcquireImage(PathID<Image> source);
		[[nodiscard]] ImageHandle AddImage(Image&& image);
		[[nodiscard]] const Image& GetImage(ImageHandle handle) const;
		[[nodiscard]] bool ReleaseImage(ImageHandle handle, Image& outImage);

	private:
		struct CachedModel
		{
			Model model;
			dU32  refCount{ 0 };
		};

		struct CachedImage
		{
			Image image;
			dU32  refCount{ 0 };
		};

	private:
		HandleAllocator<Model>               m_modelHandles;
		ResourcePool<CachedModel, Model>     m_models;
		dHashMap<PathID<Model>, ModelHandle> m_modelLookup;
		HandleAllocator<Image>               m_imageHandles;
		ResourcePool<CachedImage, Image>     m_images;
		dHashMap<PathID<Image>, ImageHandle> m_imageLookup;
	};
}
