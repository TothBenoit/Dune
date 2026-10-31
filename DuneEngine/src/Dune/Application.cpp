#include "pch.h"
#include "Dune/Application.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/ForwardPipeline.h"
#include "Dune/Game/GameScene.h"
#include "Dune/Assets/Model.h"
#include "Dune/Assets/Import/ModelImporter.h"
#include "Dune/Assets/Import/DDSImporter.h"
#include "Dune/Game/Component/Transform.h"

namespace Dune
{
	template<typename T>
	static void RemoveHandle(dVector<Handle<T>>& handles, Handle<T> handle)
	{
		const auto it = std::find(handles.begin(), handles.end(), handle);
		Assert(it != handles.end());
		*it = handles.back();
		handles.pop_back();
	}

	void Application::Initialize(const ApplicationDesc& desc)
	{
		Job::Initialize(desc.workerCount);
		FileSystem::Initialize(desc.engineResourcePath.c_str());
		for (const FileSystemRoot& root : desc.roots)
			FileSystem::AddRoot(root.name.c_str(), root.path.c_str());
		m_graphics.Initialize(desc.physicalDeviceIndex);
		m_assetCache.Initialize();
		m_lastFrameTime = std::chrono::steady_clock::now();
	}

	void Application::Destroy()
	{
		while (!m_activeViews.empty())
			DestroyView(m_activeViews.back());
		m_views.Destroy();
		m_viewHandles.Destroy();

		while (!m_activeWorlds.empty())
			DestroyWorld(m_activeWorlds.back());
		m_worlds.Destroy();
		m_worldHandles.Destroy();

		m_assetCache.Destroy();
		m_graphics.Destroy();
		FileSystem::Shutdown();
		Job::Shutdown();
	}

	ViewHandle Application::CreateView(const ViewDesc& desc)
	{
		const ViewHandle handle = m_viewHandles.Allocate();
		View& view = m_views.Resolve(handle.GetIndex());
		view.window.Initialize({ .width = desc.width, .height = desc.height, .title = desc.title });
		view.resizeEventHandle = view.window.RegisterEvent(&view.renderer, [](void* pData, const WindowMessage& message)
			{
				static_cast<Graphics::Renderer*>(pData)->OnResize(message.resize.width, message.resize.height);
			}, EWindowMessageType::Resize);
		if (desc.pRendererDesc)
			m_graphics.InitializeRenderer(view.renderer, view.window, *desc.pRendererDesc);
		else
			m_graphics.InitializeRenderer(view.renderer, view.window, Graphics::ForwardPipeline::MakeDesc());
		if (desc.enableImGui)
		{
			const FilePath layoutPath = desc.imguiLayoutPath.empty() ? FilePath{} : FileSystem::ResolvePath(desc.imguiLayoutPath.c_str());
			view.renderer.InitializeImGui(layoutPath.empty() ? nullptr : layoutPath.c_str());
		}

		m_activeViews.push_back(handle);
		return handle;
	}

	void Application::DestroyView(ViewHandle view)
	{
		View& v = GetView(view);
		v.renderer.Destroy();
		v.window.UnregisterEvent(EWindowMessageType::Resize, v.resizeEventHandle);
		v.window.Destroy();
		RemoveHandle(m_activeViews, view);
		m_viewHandles.Free(view);
	}

	WorldHandle Application::CreateWorld(const WorldDesc& desc)
	{
		const WorldHandle handle = m_worldHandles.Allocate();
		m_worlds.Resolve(handle.GetIndex()).Initialize(m_graphics, desc, m_frameContext.frameNumber);
		m_activeWorlds.push_back(handle);
		return handle;
	}

	void Application::DestroyWorld(WorldHandle world)
	{
		GetWorld(world).Destroy();
		RemoveHandle(m_activeWorlds, world);
		m_worldHandles.Free(world);
	}

	World& Application::GetWorld(WorldHandle world)
	{
		Assert(m_worldHandles.IsValid(world));
		return m_worlds[world.GetIndex()];
	}

	bool Application::BeginFrame()
	{
		for (dU32 index = (dU32)m_activeViews.size(); index-- > 0;)
		{
			const ViewHandle view = m_activeViews[index];
			if (!GetView(view).window.Update())
				DestroyView(view);
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		m_frameContext.frameNumber++;
		m_frameContext.deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
		m_lastFrameTime = now;

		GameFrame& frame = m_frames[m_frameContext.frameNumber % kFrameRingSize];
		frame.context = m_frameContext;
		frame.requests.clear();
		return !m_activeViews.empty();
	}

	void Application::Update(WorldHandle world)
	{
		GetWorld(world).Update(m_frameContext);
	}

	void Application::Render(ViewHandle view, WorldHandle world, const Camera& camera)
	{
		Assert(IsViewActive(view) && m_worldHandles.IsValid(world));
		m_frames[m_frameContext.frameNumber % kFrameRingSize].requests.push_back({ view, world, World::MakeView(camera) });
	}

	void Application::RenderFrame(dU64 frameNumber)
	{
		const GameFrame& frame = m_frames[frameNumber % kFrameRingSize];
		for (const RenderRequest& request : frame.requests)
		{
			if (!IsViewActive(request.view) || !m_worldHandles.IsValid(request.world))
				continue;
			View& view = GetView(request.view);
			m_graphics.Render(view.renderer, GetWorld(request.world).GetRenderScene(), request.viewData, frame.context, view.frameData);
		}
	}

	void Application::EndFrame()
	{
		RenderFrame(m_frameContext.frameNumber);
#ifdef _DEBUG
		for (WorldHandle world : m_activeWorlds)
			GetWorld(world).ValidateRenderScene(m_frameContext.frameNumber);
#endif
		m_graphics.EndFrame();
	}

	ModelHandle Application::LoadModel(const char* pPath)
	{
		const PathID<Model> source = FileSystem::Resolve<Model>(pPath);
		const ModelHandle cached = m_assetCache.AcquireModel(source);
		if (cached.IsValid())
			return cached;

		ModelData modelData;
		if (!ModelImporter::Import(pPath, modelData))
			return {};

		Model asset{ .source = source };
		dVector<TextureHandle> textures;
		for (const ModelImage& modelImage : modelData.images)
		{
			const ImageHandle image = LoadImage(modelImage.source);
			asset.images.push_back(image);
			textures.push_back(m_graphics.GetImageTexture(image));
		}

		asset.nodes = modelData.nodes;
		const ModelHandle model = m_assetCache.AddModel(std::move(asset));
		const Graphics::ModelResources& modelResources = m_graphics.CreateModelResources(model, modelData, textures);

		for (MaterialHandle material : modelResources.materials)
		{
			const Graphics::MaterialKey key = m_graphics.GetMaterialKey(material);
			for (ViewHandle view : m_activeViews)
				GetView(view).renderer.ResolvePSOs(key);
		}
		return model;
	}

	void Application::UnloadModel(ModelHandle model)
	{
		Model asset;
		if (!model.IsValid() || !m_assetCache.ReleaseModel(model, asset))
			return;

		m_graphics.ReleaseModelResources(model);
		for (ImageHandle image : asset.images)
			UnloadImage(image);
	}

	ImageHandle Application::LoadImage(const char* pPath)
	{
		return LoadImage(FileSystem::Resolve<Image>(pPath));
	}

	ImageHandle Application::LoadImage(PathID<Image> source)
	{
		const ImageHandle cached = m_assetCache.AcquireImage(source);
		if (cached.IsValid())
			return cached;

		ImageData data;
		if (!DDSImporter::Import(FileSystem::GetPath(source).c_str(), data))
			return {};

		const ImageHandle image = m_assetCache.AddImage({ .source = source, .format = data.format, .width = data.width, .height = data.height, .mipLevels = data.mipLevels });
		m_graphics.CreateImageTexture(image, data);
		return image;
	}

	void Application::UnloadImage(ImageHandle image)
	{
		Image asset;
		if (!image.IsValid() || !m_assetCache.ReleaseImage(image, asset))
			return;

		m_graphics.ReleaseImageTexture(image);
	}

	const Image& Application::GetImage(ImageHandle image) const
	{
		return m_assetCache.GetImage(image);
	}

	void Application::Spawn(WorldHandle world, ModelHandle model)
	{
		GameScene& scene = GetWorld(world).GetGameScene();
		const Model& asset = m_assetCache.GetModel(model);
		const Graphics::ModelResources& resources = m_graphics.GetModelResources(model);
		for (dU32 nodeIndex = 0; nodeIndex < (dU32)asset.nodes.size(); nodeIndex++)
		{
			const ModelNode& node = asset.nodes[nodeIndex];
			const Transform transform
			{
				.position = node.position,
				.rotation = DirectX::XMLoadFloat4(&node.rotation),
				.scale = 1.0f,
			};
			scene.SpawnMesh("Mesh", transform, resources.meshes[node.mesh], resources.nodeMaterialSets[nodeIndex]);
		}
	}

	ModelHandle Application::LoadModel(WorldHandle world, const char* pPath)
	{
		const ModelHandle model = LoadModel(pPath);
		if (model.IsValid())
			Spawn(world, model);
		return model;
	}

	bool Application::IsViewActive(ViewHandle view) const
	{
		return m_viewHandles.IsValid(view);
	}

	Window& Application::GetWindow(ViewHandle view)
	{
		return GetView(view).window;
	}

	RHI::ImGuiContext& Application::GetImGui(ViewHandle view)
	{
		return GetView(view).renderer.GetImGui();
	}

	View& Application::GetView(ViewHandle view)
	{
		Assert(IsViewActive(view));
		return m_views[view.GetIndex()];
	}
}
