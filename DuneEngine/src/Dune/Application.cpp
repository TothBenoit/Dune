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
		for (ViewHandle view = 0; view < (ViewHandle)m_views.size(); view++)
		{
			if (m_views[view])
				DestroyView(view);
		}
		m_views.clear();

		for (dU32 index = 0; index < m_worldCapacity; index++)
		{
			World& world = m_worlds[index];
			if (world.IsInitialized())
				DestroyWorld(m_worldHandles.GetHandleAt(index));
		}
		m_worlds.Destroy();
		m_worldHandles.Destroy();
		m_worldCapacity = 0;

		m_assetCache.Destroy();
		m_graphics.Destroy();
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
		if (desc.pRendererDesc)
			m_graphics.InitializeRenderer(pView->renderer, pView->window, *desc.pRendererDesc);
		else
			m_graphics.InitializeRenderer(pView->renderer, pView->window, Graphics::ForwardPipeline::MakeDesc());
		if (desc.enableImGui)
		{
			const FilePath layoutPath = desc.imguiLayoutPath.empty() ? FilePath{} : FileSystem::ResolvePath(desc.imguiLayoutPath.c_str());
			pView->renderer.InitializeImGui(layoutPath.empty() ? nullptr : layoutPath.c_str());
		}

		m_views.push_back(std::move(pView));
		return (ViewHandle)m_views.size() - 1;
	}

	void Application::DestroyView(ViewHandle view)
	{
		View& v = GetView(view);
		v.renderer.Destroy();
		v.window.UnregisterEvent(EWindowMessageType::Resize, v.resizeEventHandle);
		v.window.Destroy();
		m_views[view].reset();
	}

	WorldHandle Application::CreateWorld(const WorldDesc& desc)
	{
		const WorldHandle handle = m_worldHandles.Allocate();
		const dU32 index = handle.GetIndex();
		m_worldCapacity = std::max(m_worldCapacity, index + 1);
		m_worlds.Resolve(index).Initialize(m_graphics, desc);
		return handle;
	}

	void Application::DestroyWorld(WorldHandle world)
	{
		GetWorld(world).Destroy();
		m_worldHandles.Free(world);
	}

	World& Application::GetWorld(WorldHandle world)
	{
		Assert(m_worldHandles.IsValid(world));
		return m_worlds[world.GetIndex()];
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
		return !m_views.empty() && m_views[0];
	}

	void Application::Update(WorldHandle world)
	{
		GetWorld(world).Update();
	}

	void Application::Render(ViewHandle view, WorldHandle world, const Camera& camera)
	{
		View& v = GetView(view);
		m_graphics.Render(v.renderer, GetWorld(world).GetRenderScene(), World::MakeView(camera), m_deltaTime, v.frameData);
	}

	void Application::EndFrame()
	{
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

	bool Application::IsViewOpen(ViewHandle view) const
	{
		return view < (ViewHandle)m_views.size() && m_views[view];
	}

	Window& Application::GetWindow(ViewHandle view)
	{
		return GetView(view).window;
	}

	RHI::ImGuiContext& Application::GetImGui(ViewHandle view)
	{
		return GetView(view).renderer.GetImGui();
	}

	Application::View& Application::GetView(ViewHandle view)
	{
		Assert(IsViewOpen(view));
		return *m_views[view];
	}
}
