#include "pch.h"
#include "Dune/Application.h"
#include "Dune/Core/JobSystem.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/SceneRender.h"
#include "Dune/Graphics/ModelData.h"
#include "Dune/Utilities/ModelImporter.h"
#include "Dune/Utilities/SceneLoader.h"

namespace Dune
{
	void Application::Initialize(const ApplicationDesc& desc)
	{
		Job::Initialize(desc.workerCount);
		FileSystem::Initialize(desc.engineResourcePath.c_str());
		for (const FileSystemRoot& root : desc.roots)
			FileSystem::AddRoot(root.name.c_str(), root.path.c_str());
		m_renderContext.Initialize();
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
			pView->imgui.Initialize(pView->window, pView->renderer);
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

	void Application::LoadModel(Scene& scene, const char* pPath)
	{
		Graphics::ModelData model;
		if (!ModelImporter::Import(pPath, model))
			return;

		Graphics::ResourceManager& resources = m_renderContext.GetResourceManager();
		Graphics::UploadContext& uploadContext = m_renderContext.GetUploadContext();
		const Graphics::ModelResources modelResources = resources.CreateModel(model);
		const Graphics::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(modelResources), Graphics::Texture::kUploadAlignment);
		resources.Upload(modelResources, model, uploadContext.GetCommandList(), staging);
		SceneLoader::Spawn(scene, model, modelResources);

		for (const std::unique_ptr<View>& pView : m_views)
		{
			if (!pView)
				continue;
			for (Graphics::MaterialHandle material : modelResources.materials)
				pView->renderer.ResolvePSOs(resources.GetMaterial(material).GetKey());
		}
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
