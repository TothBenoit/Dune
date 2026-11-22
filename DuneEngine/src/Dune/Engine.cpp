#include "pch.h"
#include "Dune/Engine.h"
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
	static dUVec2 GetViewSize(const WorldView& view)
	{
		const dU32 windowWidth = view.pWindow->GetWidth();
		const dU32 windowHeight = view.pWindow->GetHeight();
		return
		{
			view.width ? view.width : (windowWidth > view.x ? windowWidth - view.x : 0),
			view.height ? view.height : (windowHeight > view.y ? windowHeight - view.y : 0),
		};
	}

	void Run(Application& application, const EngineDesc& desc)
	{
		Engine engine{};
		engine.Initialize(desc);
		engine.Run(application);
		engine.Destroy();
	}

	void Engine::Initialize(const EngineDesc& desc)
	{
		Job::Initialize(desc.workerCount);
		FileSystem::Initialize(desc.engineResourcePath.c_str());
		for (const FileSystemRoot& root : desc.roots)
			FileSystem::AddRoot(root.name.c_str(), root.path.c_str());
		m_graphics.Initialize(desc.physicalDeviceIndex);
		m_assetCache.Initialize();
		m_osThreadID = std::this_thread::get_id();
	}

	void Engine::Destroy()
	{
		while (!m_worldViews.empty())
			DestroyWorldView(m_worldViews.back());
		while (!m_windows.empty())
			DestroyWindow(&m_windows.back()->window);

		while (!m_worlds.empty())
			DestroyWorld(m_worlds.back());

		m_assetCache.Destroy();
		m_graphics.Destroy();
		FileSystem::Shutdown();
		Job::Shutdown();
	}

	Window* Engine::CreateWindow(const WindowDesc& desc, const Graphics::WindowSurfaceDesc& surfaceDesc)
	{
		Assert(std::this_thread::get_id() == m_osThreadID);
		WindowEntry* pEntry = new WindowEntry();
		pEntry->window.Initialize(desc);
		const FilePath layoutPath = surfaceDesc.pImGuiLayoutPath ? FileSystem::ResolvePath(surfaceDesc.pImGuiLayoutPath) : FilePath{};
		pEntry->surface.Initialize(m_graphics.GetRenderContext(), pEntry->window, { .enableImGui = surfaceDesc.enableImGui, .pImGuiLayoutPath = layoutPath.empty() ? nullptr : layoutPath.c_str() });
		m_windows.push_back(pEntry);
		return &pEntry->window;
	}

	void Engine::DestroyWindow(Window* pWindow)
	{
		Assert(std::this_thread::get_id() == m_osThreadID);
		for (const WorldView* pView : m_worldViews)
			Assert(pView->pWindow != pWindow);
		WindowEntry* pEntry = &GetWindowEntry(*pWindow);
		pEntry->surface.Destroy();
		pEntry->window.Destroy();
		SwapRemove(m_windows, pEntry);
		delete pEntry;
	}

	WorldView* Engine::CreateWorldView(const WorldViewDesc& desc)
	{
		Assert(desc.pWorld && desc.pWindow);
		WorldView* pView = new WorldView{ .pWorld = desc.pWorld, .camera = desc.camera, .pWindow = desc.pWindow, .x = desc.x, .y = desc.y, .width = desc.width, .height = desc.height };
		pView->renderer.Initialize(m_graphics.GetRenderContext(), desc.pRendererDesc ? *desc.pRendererDesc : Graphics::ForwardPipeline::MakeDesc());
		m_worldViews.push_back(pView);
		return pView;
	}

	void Engine::DestroyWorldView(WorldView* pView)
	{
		pView->renderer.Destroy();
		SwapRemove(m_worldViews, pView);
		delete pView;
	}

	World* Engine::CreateWorld(const WorldDesc& desc)
	{
		World* pWorld = new World();
		pWorld->Initialize(m_graphics, desc, m_frameContext.frameNumber);
		m_worlds.push_back(pWorld);
		return pWorld;
	}

	void Engine::DestroyWorld(World* pWorld)
	{
		for (const WorldView* pView : m_worldViews)
			Assert(pView->pWorld != pWorld);
		pWorld->Destroy();
		SwapRemove(m_worlds, pWorld);
		delete pWorld;
	}

	void Engine::Run(Application& application)
	{
		Assert(std::this_thread::get_id() == m_osThreadID);
		application.Init(*this);

		m_lastFrameTime = std::chrono::steady_clock::now();
		bool isRunning = true;
		while (isRunning)
		{
			PrepareTick();

			Job::JobBuilder builder;
			builder.DispatchJob([this, &application, &isRunning]() { RunTick(application, isRunning); });
			const Job::Counter& counter = builder.ExtractWaitCounter();
			while (counter.GetValue() > 0)
			{
				Window::ProcessBlockingMessages();
				std::this_thread::yield();
			}
		}

		application.Shutdown(*this);
	}

	void Engine::PrepareTick()
	{
		for (WindowEntry* pEntry : m_windows)
		{
			pEntry->window.ProcessMessages();
			pEntry->window.Update();
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		m_frameContext.frameNumber++;
		m_frameContext.deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
		m_lastFrameTime = now;
	}

	void Engine::RunTick(Application& application, bool& isRunning)
	{
		isRunning = application.Tick(*this, m_frameContext.deltaTime);

		Job::JobBuilder builder;
		for (World* pWorld : m_worlds)
			builder.DispatchJob<Job::Fence::None>([this, pWorld]() { pWorld->Update(m_frameContext); });
		Job::WaitForCounter(builder.ExtractWaitCounter());

		RenderFrame();
#ifdef _DEBUG
		for (World* pWorld : m_worlds)
			pWorld->ValidateRenderScene(m_frameContext.frameNumber);
#endif
		m_graphics.EndFrame();
	}

	void Engine::RenderFrame()
	{
		for (WorldView* pView : m_worldViews)
			m_graphics.Render(pView->renderer, pView->pWorld->GetRenderScene(), pView->pWorld->MakeView(pView->camera, GetViewSize(*pView)), m_frameContext, pView->frameData);

		for (WindowEntry* pEntry : m_windows)
		{
			m_blits.clear();
			for (WorldView* pView : m_worldViews)
			{
				if (pView->pWindow == &pEntry->window)
					m_blits.push_back({ .pTexture = &pView->renderer.GetOutputTexture(), .x = pView->x, .y = pView->y });
			}
			pEntry->surface.Present(m_blits);
		}
	}

	ModelHandle Engine::LoadModel(const char* pPath)
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
			for (WorldView* pView : m_worldViews)
				pView->renderer.ResolvePSOs(key);
		}
		return model;
	}

	void Engine::UnloadModel(ModelHandle model)
	{
		Model asset;
		if (!model.IsValid() || !m_assetCache.ReleaseModel(model, asset))
			return;

		m_graphics.ReleaseModelResources(model);
		for (ImageHandle image : asset.images)
			UnloadImage(image);
	}

	ImageHandle Engine::LoadImage(const char* pPath)
	{
		return LoadImage(FileSystem::Resolve<Image>(pPath));
	}

	ImageHandle Engine::LoadImage(PathID<Image> source)
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

	void Engine::UnloadImage(ImageHandle image)
	{
		Image asset;
		if (!image.IsValid() || !m_assetCache.ReleaseImage(image, asset))
			return;

		m_graphics.ReleaseImageTexture(image);
	}

	const Image& Engine::GetImage(ImageHandle image) const
	{
		return m_assetCache.GetImage(image);
	}

	void Engine::Spawn(World& world, ModelHandle model)
	{
		GameScene& scene = world.GetGameScene();
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

	ModelHandle Engine::LoadModel(World& world, const char* pPath)
	{
		const ModelHandle model = LoadModel(pPath);
		if (model.IsValid())
			Spawn(world, model);
		return model;
	}

	RHI::ImGuiContext& Engine::GetImGui(const Window& window)
	{
		return GetWindowEntry(window).surface.GetImGui();
	}

	Engine::WindowEntry& Engine::GetWindowEntry(const Window& window)
	{
		for (WindowEntry* pEntry : m_windows)
		{
			if (&pEntry->window == &window)
				return *pEntry;
		}
		Assert(false);
		return *m_windows.front();
	}
}
