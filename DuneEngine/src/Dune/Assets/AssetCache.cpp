#include "pch.h"
#include "Dune/Assets/AssetCache.h"

namespace Dune
{
	void AssetCache::Initialize()
	{
		m_models.Initialize(m_modelHandles);
		m_images.Initialize(m_imageHandles);
	}

	void AssetCache::Destroy()
	{
		Assert(m_modelLookup.empty() && m_imageLookup.empty());

		m_imageLookup.clear();
		m_images.Destroy();
		m_imageHandles.Destroy();
		m_modelLookup.clear();
		m_models.Destroy();
		m_modelHandles.Destroy();
	}

	ModelHandle AssetCache::AcquireModel(FileSystem::SerializationID<EFileType::Model> source)
	{
		auto it = m_modelLookup.find(source);
		if (it == m_modelLookup.end())
			return {};
		m_models.Get(it->second).refCount++;
		return it->second;
	}

	ModelHandle AssetCache::AddModel(Model&& model)
	{
		Assert(model.source.IsValid() && m_modelLookup.find(model.source) == m_modelLookup.end());
		const ModelHandle handle = m_modelHandles.Allocate();
		m_modelLookup.emplace(model.source, handle);
		m_models.Insert(handle, { .model = std::move(model), .refCount = 1 });
		return handle;
	}

	const Model& AssetCache::GetModel(ModelHandle handle) const
	{
		return m_models.Get(handle).model;
	}

	bool AssetCache::ReleaseModel(ModelHandle handle, Model& outModel)
	{
		CachedModel& cached = m_models.Get(handle);
		Assert(cached.refCount > 0);
		if (--cached.refCount > 0)
			return false;

		m_modelLookup.erase(cached.model.source);
		outModel = m_models.Remove(handle).model;
		m_modelHandles.Free(handle);
		return true;
	}

	ImageHandle AssetCache::AcquireImage(FileSystem::SerializationID<EFileType::Image> source)
	{
		auto it = m_imageLookup.find(source);
		if (it == m_imageLookup.end())
			return {};
		m_images.Get(it->second).refCount++;
		return it->second;
	}

	ImageHandle AssetCache::AddImage(Image&& image)
	{
		Assert(image.source.IsValid() && m_imageLookup.find(image.source) == m_imageLookup.end());
		const ImageHandle handle = m_imageHandles.Allocate();
		m_imageLookup.emplace(image.source, handle);
		m_images.Insert(handle, { .image = std::move(image), .refCount = 1 });
		return handle;
	}

	const Image& AssetCache::GetImage(ImageHandle handle) const
	{
		return m_images.Get(handle).image;
	}

	bool AssetCache::ReleaseImage(ImageHandle handle, Image& outImage)
	{
		CachedImage& cached = m_images.Get(handle);
		Assert(cached.refCount > 0);
		if (--cached.refCount > 0)
			return false;

		m_imageLookup.erase(cached.image.source);
		outImage = m_images.Remove(handle).image;
		m_imageHandles.Free(handle);
		return true;
	}
}
