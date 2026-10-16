#include "pch.h"
#include "Dune/Application.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/SceneRender.h"
#include "Dune/Utilities/SceneLoader.h"
#include "Dune/Utilities/ModelImporter.h"

namespace Dune
{
	void Application::Initialize(const ApplicationDesc& desc)
	{
		Job::Initialize(desc.workerCount);
		FileSystem::Initialize(desc.engineResourcePath.c_str());
		for (const FileSystemRoot& root : desc.roots)
			FileSystem::AddRoot(root.name.c_str(), root.path.c_str());
		m_renderContext.Initialize(&m_resourceHandles);
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

		m_assetCache.Destroy();
		m_renderContext.Destroy();
		FileSystem::Shutdown();
		Job::Shutdown();
	}

	ViewHandle Application::CreateView(const ViewDesc& desc)
	{
		std::unique_ptr<View> pView = std::make_unique<View>();
		pView->window.Initialize({ .width = desc.width, .height = desc.height, .title = desc.title });
		pView->resizeEventHandle = pView->window.RegisterEvent(&pView->renderer, [](void* pData, const Graphics::WindowMessage& message)
			{
				static_cast<Graphics::Renderer*>(pData)->OnResize(message.resize.width, message.resize.height);
			}, Graphics::EWindowMessageType::Resize);
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
		v.window.UnregisterEvent(Graphics::EWindowMessageType::Resize, v.resizeEventHandle);
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
		m_sceneFrameCount = 0;
	}

	ModelHandle Application::LoadModel(const char* pPath)
	{
		const ModelHandle cached = m_assetCache.FindModel(pPath);
		if (cached.IsValid())
			return cached;

		Graphics::ModelData modelData;
		if (!ModelImporter::Import(pPath, modelData))
			return {};

		const Graphics::ModelResources modelResources = CreateModel(modelData);
		Model asset{ .materials = modelResources.materials };
		for (dU32 nodeIndex = 0; nodeIndex < (dU32)modelData.nodes.size(); nodeIndex++)
		{
			const Graphics::ModelNode& node = modelData.nodes[nodeIndex];
			asset.nodes.push_back({ .position = node.position, .rotation = node.rotation, .mesh = modelResources.meshes[node.mesh], .materials = modelResources.nodeMaterialSets[nodeIndex] });
		}
		const ModelHandle model = m_assetCache.AddModel(pPath, std::move(asset));

		const Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		for (Graphics::MaterialHandle material : m_assetCache.GetModel(model).materials)
		{
			const Graphics::MaterialKey key = resources.GetMaterial(material).GetKey();
			for (const std::unique_ptr<View>& pView : m_views)
			{
				if (pView)
					pView->renderer.ResolvePSOs(key);
			}
		}
		return model;
	}

	void Application::Spawn(Scene& scene, ModelHandle model)
	{
		SceneLoader::Spawn(scene, m_assetCache.GetModel(model));
	}

	ModelHandle Application::LoadModel(Scene& scene, const char* pPath)
	{
		const ModelHandle model = LoadModel(pPath);
		if (model.IsValid())
			Spawn(scene, model);
		return model;
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

	Graphics::ModelResources Application::CreateModel(const Graphics::ModelData& model)
	{
		const Graphics::ModelResources handles = m_resourceHandles.AllocateModel(model);
		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateModel(handles, model);
		const Graphics::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handles), Graphics::Texture::kUploadAlignment);
		resources.Upload(handles, model, uploadContext.GetCommandList(), staging);
		return handles;
	}

	bool Application::IsViewOpen(ViewHandle view) const
	{
		return view < (ViewHandle)m_views.size() && m_views[view];
	}

	Graphics::Window& Application::GetWindow(ViewHandle view)
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
