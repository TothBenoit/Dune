#include "pch.h"
#include "Dune/Application.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/World/Scene.h"
#include "Dune/World/SceneRender.h"
#include "Dune/Assets/Model.h"
#include "Dune/Assets/Import/ModelImporter.h"
#include "Dune/Assets/Import/DDSImporter.h"
#include "Dune/Graphics/MeshUtils.h"

namespace Dune
{
	void Application::Initialize(const ApplicationDesc& desc)
	{
		Job::Initialize(desc.workerCount);
		FileSystem::Initialize(desc.engineResourcePath.c_str());
		for (const FileSystemRoot& root : desc.roots)
			FileSystem::AddRoot(root.name.c_str(), root.path.c_str());
		m_renderContext.Initialize(m_resourceHandles);
		m_assetCache.Initialize();
		for (dU32 primitive = 0; primitive < (dU32)EPrimitive::Count; primitive++)
			m_primitives[primitive] = CreateMesh(MeshUtils::CreatePrimitive((EPrimitive)primitive));
		m_defaultMaterial = CreateMaterial({});
		m_defaultMaterialSet = CreateMaterialSet({ m_defaultMaterial });
		m_lastFrameTime = std::chrono::steady_clock::now();
	}

	void Application::Destroy()
	{
		for (ViewHandle view = 0; view < (ViewHandle)m_views.size(); view++)
		{
			if (m_views[view])
				DestroyView(view);
		}
		m_views.clear();
		m_sceneFrames.clear();

		m_pendingTextures.clear();
		m_pendingMeshes.clear();
		m_pendingMaterialSets.clear();
		m_assetCache.Destroy();
		m_renderContext.Destroy();
		FileSystem::Shutdown();
		Job::Shutdown();
	}

	ViewHandle Application::CreateView(const ViewDesc& desc)
	{
		std::unique_ptr<View> pView = std::make_unique<View>();
		pView->window.Initialize({ .width = desc.width, .height = desc.height, .title = desc.title });
		pView->resizeEventHandle = pView->window.RegisterEvent(&pView->renderer, [](void* pData, const WindowMessage& message)
			{
				static_cast<Graphics::Renderer*>(pData)->OnResize(message.resize.width, message.resize.height);
			}, EWindowMessageType::Resize);
		m_renderContext.InitializeRenderer(pView->renderer, pView->window);
		if (desc.enableImGui)
		{
			const FilePath layoutPath = desc.imguiLayoutPath.empty() ? FilePath{} : FileSystem::ResolvePath(desc.imguiLayoutPath.c_str());
			pView->imgui.Initialize(pView->window, pView->renderer, layoutPath.empty() ? nullptr : layoutPath.c_str());
			pView->hasImGui = true;
		}

		m_views.push_back(std::move(pView));
		return (ViewHandle)m_views.size() - 1;
	}

	void Application::DestroyView(ViewHandle view)
	{
		View& v = GetView(view);
		if (v.hasImGui)
			v.imgui.Destroy();
		v.renderer.Destroy();
		v.window.UnregisterEvent(EWindowMessageType::Resize, v.resizeEventHandle);
		v.window.Destroy();
		m_views[view].reset();
	}

	bool Application::BeginFrame()
	{
		for (ViewHandle view = 0; view < (ViewHandle)m_views.size(); view++)
		{
			if (m_views[view] && !m_views[view]->window.Update())
			{
				if (view == 0)
					return false;
				DestroyView(view);
			}
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		m_deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
		m_lastFrameTime = now;
		m_sceneFrameCount = 0;
		return !m_views.empty() && m_views[0];
	}

	void Application::Render(ViewHandle view, const Scene& scene, const Camera& camera)
	{
		m_renderContext.GetUploadContext().Submit();
		const Graphics::ViewData viewData = SceneRender::ExtractView(camera);
		GetView(view).renderer.Render(GetFrameData(scene), viewData);
	}

	void Application::EndFrame()
	{
		m_renderContext.GetUploadContext().Submit();
		m_renderContext.Signal();
		ReleasePendingResources();
		m_sceneFrameCount = 0;
	}

	ModelHandle Application::LoadModel(const char* pPath)
	{
		const FileSystem::SerializationID<EFileType::Model> source = FileSystem::Resolve<EFileType::Model>(pPath);
		const ModelHandle cached = m_assetCache.AcquireModel(source);
		if (cached.IsValid())
			return cached;

		ModelData modelData;
		if (!ModelImporter::Import(pPath, modelData))
			return {};

		Model asset{ .source = source };
		dVector<Graphics::TextureHandle> textures;
		for (const ModelImage& modelImage : modelData.images)
		{
			const ImageHandle image = LoadImage(modelImage.source);
			asset.images.push_back(image);
			textures.push_back(m_resourceHandles.GetTexture(image));
		}

		const Graphics::ModelResources modelResources = CreateModel(modelData, textures);
		asset.nodes = std::move(modelData.nodes);
		const ModelHandle model = m_assetCache.AddModel(std::move(asset));
		m_resourceHandles.modelResources.emplace(model, modelResources);

		const Graphics::ResourceManager& resourceManager = m_renderContext.GetResourceManager();
		for (Graphics::MaterialHandle material : modelResources.materials)
		{
			const Graphics::MaterialKey key = resourceManager.GetMaterial(material).GetKey();
			for (const std::unique_ptr<View>& pView : m_views)
			{
				if (pView)
					pView->renderer.ResolvePSOs(key);
			}
		}
		return model;
	}

	void Application::UnloadModel(ModelHandle model)
	{
		Model asset;
		if (!model.IsValid() || !m_assetCache.ReleaseModel(model, asset))
			return;

		auto it = m_resourceHandles.modelResources.find(model);
		Assert(it != m_resourceHandles.modelResources.end());
		const Graphics::ModelResources modelResources = std::move(it->second);
		m_resourceHandles.modelResources.erase(it);

		for (Graphics::MaterialSetHandle materialSet : modelResources.nodeMaterialSets)
			DestroyMaterialSet(materialSet);
		for (Graphics::MaterialHandle material : modelResources.materials)
			DestroyMaterial(material);
		for (Graphics::MeshHandle mesh : modelResources.meshes)
			DestroyMesh(mesh);
		for (ImageHandle image : asset.images)
			UnloadImage(image);
	}

	ImageHandle Application::LoadImage(const char* pPath)
	{
		return LoadImage(FileSystem::Resolve<EFileType::Image>(pPath));
	}

	ImageHandle Application::LoadImage(FileSystem::SerializationID<EFileType::Image> source)
	{
		const ImageHandle cached = m_assetCache.AcquireImage(source);
		if (cached.IsValid())
			return cached;

		ImageData data;
		if (!DDSImporter::Import(FileSystem::GetPath(source).c_str(), data))
			return {};

		const ImageHandle image = m_assetCache.AddImage({ .source = source, .format = data.format, .width = data.width, .height = data.height, .mipLevels = data.mipLevels });
		m_resourceHandles.imageTextures.emplace(image, CreateTexture(data));
		return image;
	}

	void Application::UnloadImage(ImageHandle image)
	{
		Image asset;
		if (!image.IsValid() || !m_assetCache.ReleaseImage(image, asset))
			return;

		auto it = m_resourceHandles.imageTextures.find(image);
		Assert(it != m_resourceHandles.imageTextures.end());
		DestroyTexture(it->second);
		m_resourceHandles.imageTextures.erase(it);
	}

	const Image& Application::GetImage(ImageHandle image) const
	{
		return m_assetCache.GetImage(image);
	}

	void Application::Spawn(Scene& scene, ModelHandle model)
	{
		scene.Spawn(m_assetCache.GetModel(model), m_resourceHandles.GetModelResources(model));
	}

	ModelHandle Application::LoadModel(Scene& scene, const char* pPath)
	{
		const ModelHandle model = LoadModel(pPath);
		if (model.IsValid())
			Spawn(scene, model);
		return model;
	}

	EntityID Application::SpawnPrimitive(Scene& scene, EPrimitive primitive, const Transform& transform, Graphics::MaterialSetHandle materials)
	{
		static constexpr const char* kPrimitiveNames[]{ "Cube", "Sphere", "Plane" };
		static_assert(_countof(kPrimitiveNames) == (dU32)EPrimitive::Count);

		return scene.SpawnMesh(kPrimitiveNames[(dU32)primitive], transform, m_primitives[(dU32)primitive], materials.IsValid() ? materials : m_defaultMaterialSet);
	}

	Graphics::TextureHandle Application::CreateTexture(const Graphics::TextureData& data)
	{
		const Graphics::TextureHandle handle = m_resourceHandles.textures.Allocate();
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, data);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Graphics::Texture::kUploadAlignment);
		resources.Upload(handle, data, uploadContext.GetCommandList(), staging);
		return handle;
	}

	Graphics::TextureHandle Application::CreateTexture(const ImageData& image)
	{
		const Graphics::TextureHandle handle = m_resourceHandles.textures.Allocate();
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, image);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Graphics::Texture::kUploadAlignment);
		resources.Upload(handle, image, uploadContext.GetCommandList(), staging);
		return handle;
	}

	Graphics::MeshHandle Application::CreateMesh(const Graphics::MeshData& meshData)
	{
		const Graphics::MeshHandle handle = m_resourceHandles.meshes.Allocate();
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateMesh(handle, meshData);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Graphics::Mesh::kUploadAlignment);
		resources.Upload(handle, meshData, uploadContext.GetCommandList(), staging);
		return handle;
	}

	Graphics::MaterialHandle Application::CreateMaterial(const Graphics::MaterialDesc& desc)
	{
		const Graphics::MaterialHandle handle = m_resourceHandles.materials.Allocate();
		m_renderContext.GetResourceManager().CreateMaterial(handle, desc);
		return handle;
	}

	Graphics::MaterialSetHandle Application::CreateMaterialSet(dSpan<Graphics::MaterialHandle> materials)
	{
		const Graphics::MaterialSetHandle handle = m_resourceHandles.materialSets.Allocate();
		m_renderContext.GetResourceManager().CreateMaterialSet(handle, materials);
		return handle;
	}

	Graphics::ModelResources Application::CreateModel(const ModelData& model, dSpan<Graphics::TextureHandle> images)
	{
		const Graphics::ModelResources handles = m_resourceHandles.AllocateModel(model);
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateModel(handles, model, images);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handles), Graphics::Mesh::kUploadAlignment);
		resources.Upload(handles, model, uploadContext.GetCommandList(), staging);
		return handles;
	}

	void Application::DestroyTexture(Graphics::TextureHandle texture)
	{
		m_pendingTextures.push_back({ .handle = texture, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void Application::DestroyMesh(Graphics::MeshHandle mesh)
	{
		m_pendingMeshes.push_back({ .handle = mesh, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void Application::DestroyMaterial(Graphics::MaterialHandle material)
	{
		m_renderContext.GetResourceManager().Destroy(material);
		m_resourceHandles.materials.Free(material);
	}

	void Application::DestroyMaterialSet(Graphics::MaterialSetHandle materialSet)
	{
		m_pendingMaterialSets.push_back({ .handle = materialSet, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	template<typename TPending, typename TRelease>
	static void ReleaseCompleted(dVector<TPending>& pending, dU64 completedValue, TRelease release)
	{
		for (dU32 i = 0; i < (dU32)pending.size();)
		{
			if (pending[i].fenceValue <= completedValue)
			{
				release(pending[i].handle);
				pending[i] = pending.back();
				pending.pop_back();
			}
			else
				i++;
		}
	}

	void Application::ReleasePendingResources()
	{
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		const dU64 completedValue = m_renderContext.GetCompletedFenceValue();
		ReleaseCompleted(m_pendingTextures, completedValue, [&](Graphics::TextureHandle texture) { resources.Destroy(texture); m_resourceHandles.textures.Free(texture); });
		ReleaseCompleted(m_pendingMeshes, completedValue, [&](Graphics::MeshHandle mesh) { resources.Destroy(mesh); m_resourceHandles.meshes.Free(mesh); });
		ReleaseCompleted(m_pendingMaterialSets, completedValue, [&](Graphics::MaterialSetHandle materialSet) { resources.Destroy(materialSet); m_resourceHandles.materialSets.Free(materialSet); });
	}

	bool Application::IsViewOpen(ViewHandle view) const
	{
		return view < (ViewHandle)m_views.size() && m_views[view];
	}

	Window& Application::GetWindow(ViewHandle view)
	{
		return GetView(view).window;
	}

	Graphics::Renderer& Application::GetRenderer(ViewHandle view)
	{
		return GetView(view).renderer;
	}

	RHI::ImGuiWrapper& Application::GetImGui(ViewHandle view)
	{
		View& v = GetView(view);
		Assert(v.hasImGui);
		return v.imgui;
	}

	Application::View& Application::GetView(ViewHandle view)
	{
		Assert(IsViewOpen(view));
		return *m_views[view];
	}

	const Graphics::FrameData& Application::GetFrameData(const Scene& scene)
	{
		for (dU32 i = 0; i < m_sceneFrameCount; i++)
		{
			if (m_sceneFrames[i].pScene == &scene)
				return m_sceneFrames[i].frameData;
		}

		if (m_sceneFrameCount == m_sceneFrames.size())
			m_sceneFrames.emplace_back();
		SceneFrame& sceneFrame = m_sceneFrames[m_sceneFrameCount++];
		sceneFrame.pScene = &scene;

		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		SceneRender::Extract(scene, resources, sceneFrame.frameData);
		resources.FillFrameData(sceneFrame.frameData);
		sceneFrame.frameData.deltaTime = m_deltaTime;
		return sceneFrame.frameData;
	}
}
