#include "pch.h"
#include "Dune/Application.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/SceneRender.h"
#include "Dune/Asset/Model.h"
#include "Dune/Asset/Import/ModelImporter.h"
#include "Dune/Asset/Import/DDSImporter.h"
#include "Dune/Graphics/MeshUtils.h"
#include "Dune/Component/Name.h"
#include "Dune/Component/MeshInstance.h"

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
		GetView(view).renderer.Render(GetFrameData(scene), camera);
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

		Graphics::ModelData modelData;
		if (!ModelImporter::Import(pPath, modelData))
			return {};

		Model asset{ .source = source };
		dVector<Graphics::TextureHandle> textures;
		for (const Graphics::ModelImage& modelImage : modelData.images)
		{
			const ImageHandle image = LoadImage(modelImage.source);
			asset.images.push_back(image);
			textures.push_back(image.IsValid() ? m_assetCache.GetImage(image).texture : Graphics::TextureHandle{});
		}

		const Graphics::ModelResources modelResources = CreateModel(modelData, textures);
		asset.materials = modelResources.materials;
		asset.meshes = modelResources.meshes;
		for (dU32 nodeIndex = 0; nodeIndex < (dU32)modelData.nodes.size(); nodeIndex++)
		{
			const Graphics::ModelNode& node = modelData.nodes[nodeIndex];
			asset.nodes.push_back({ .position = node.position, .rotation = node.rotation, .mesh = modelResources.meshes[node.mesh], .materials = modelResources.nodeMaterialSets[nodeIndex] });
		}
		const ModelHandle model = m_assetCache.AddModel(std::move(asset));

		const Graphics::ResourceManager& resourceManager = m_renderContext.GetResourceManager();
		for (Graphics::MaterialHandle material : m_assetCache.GetModel(model).materials)
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

		for (const Model::Node& node : asset.nodes)
			DestroyMaterialSet(node.materials);
		for (Graphics::MaterialHandle material : asset.materials)
			DestroyMaterial(material);
		for (Graphics::MeshHandle mesh : asset.meshes)
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

		Graphics::TextureData data;
		if (!DDSImporter::Import(FileSystem::GetPath(source).c_str(), data))
			return {};

		return m_assetCache.AddImage({ .source = source, .texture = CreateTexture(data) });
	}

	void Application::UnloadImage(ImageHandle image)
	{
		Image asset;
		if (image.IsValid() && m_assetCache.ReleaseImage(image, asset))
			DestroyTexture(asset.texture);
	}

	const Image& Application::GetImage(ImageHandle image) const
	{
		return m_assetCache.GetImage(image);
	}

	void Application::Spawn(Scene& scene, ModelHandle model)
	{
		for (const Model::Node& node : m_assetCache.GetModel(model).nodes)
		{
			const EntityID entity = scene.registry.create();
			scene.registry.emplace<Name>(entity).name.assign("Mesh");

			Transform& transform = scene.registry.emplace<Transform>(entity);
			transform.position = node.position;
			transform.rotation = DirectX::XMLoadFloat4(&node.rotation);
			transform.scale = 1.0f;

			MeshInstance& meshInstance = scene.registry.emplace<MeshInstance>(entity);
			meshInstance.mesh = node.mesh;
			meshInstance.materials = node.materials;
		}
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

		const EntityID entity = scene.registry.create();
		scene.registry.emplace<Name>(entity, kPrimitiveNames[(dU32)primitive]);
		scene.registry.emplace<Transform>(entity, transform);
		scene.registry.emplace<MeshInstance>(entity, MeshInstance{ .mesh = m_primitives[(dU32)primitive], .materials = materials.IsValid() ? materials : m_defaultMaterialSet });
		return entity;
	}

	Graphics::TextureHandle Application::CreateTexture(const Graphics::TextureData& data)
	{
		const Graphics::TextureHandle handle = m_resourceHandles.textures.Allocate();
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, data);
		const Graphics::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Graphics::Texture::kUploadAlignment);
		resources.Upload(handle, data, uploadContext.GetCommandList(), staging);
		return handle;
	}

	Graphics::MeshHandle Application::CreateMesh(const Graphics::MeshData& meshData)
	{
		const Graphics::MeshHandle handle = m_resourceHandles.meshes.Allocate();
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateMesh(handle, meshData);
		const Graphics::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Graphics::Mesh::kUploadAlignment);
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

	Graphics::ModelResources Application::CreateModel(const Graphics::ModelData& model, dSpan<Graphics::TextureHandle> images)
	{
		const Graphics::ModelResources handles = m_resourceHandles.AllocateModel(model);
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateModel(handles, model, images);
		const Graphics::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handles), Graphics::Mesh::kUploadAlignment);
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

	Graphics::ImGuiWrapper& Application::GetImGui(ViewHandle view)
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
