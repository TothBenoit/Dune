#include "pch.h"
#include "GraphicsCore.h"
#include "Dune/Core/Logger.h"

namespace Dune
{

	std::unique_ptr<GraphicsRenderer> GraphicsCore::m_graphicsRenderer = nullptr;
	bool GraphicsCore::isInitialized = false;

	void GraphicsCore::Init(const void* window)
	{
		if (isInitialized)
		{
			LOG_CRITICAL("Tried to initialize GraphicsCore which is already initialized");
			return;
		}

		m_graphicsRenderer = GraphicsRenderer::Create(window);
		isInitialized = true;

		// Define the geometry for a triangle.
		dVector<Vertex> triangleVertices =
		{
			{ { 0.25f, -0.25f * 1.77f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { 0.0f, 0.25f * 1.77f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * 1.77f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};
		dVector<uint32_t> indices = {};
		Mesh* mesh = new Mesh(indices, triangleVertices);
		mesh->UploadBuffers();
		dString path = "bla";
		Shader *shader = new Shader(path);
		GraphicsElement * elem = new GraphicsElement(*mesh,*shader);

		m_graphicsRenderer->AddGraphicsElement(*elem);
	}

	void GraphicsCore::Shutdown()
	{
#ifdef _DEBUG
		if (!isInitialized)
		{
			LOG_CRITICAL("Tried to shutdown GraphicsCore which is not initialized");
			return;
		}
#endif // _DEBUG

		m_graphicsRenderer->OnShutdown();
	}

	void GraphicsCore::Update()
	{
#ifdef _DEBUG
		if (!isInitialized)
		{
			LOG_CRITICAL("Tried to update GraphicsCore which is not initialized");
			return;
		}
#endif // _DEBUG

		m_graphicsRenderer->WaitForGPU();
		m_graphicsRenderer->Render();
		m_graphicsRenderer->Present();
	}
}