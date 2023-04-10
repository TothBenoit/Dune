#include "pch.h"

#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"
#include "Dune/Core/Window.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"
#include "Dune/Graphics/PointLight.h"
#include "Dune/Graphics/DirectionalLight.h"
#include "Dune/Graphics/Mesh.h"

namespace Dune
{
	Renderer::~Renderer()
	{
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
#endif
	}

	Renderer& Renderer::GetInstance()
	{
		// The compiler adds additionnal check with this kind of lazy init singleton.
		// Should I fall back to a more traditionnal singleton ?
		static Renderer instance;
		return instance;
	}

	void Renderer::Initialize(const Window* window)
	{
		m_bIsInitialized = true;
		CreateFactory();
#ifdef _DEBUG
		EnableDebugLayer();
#endif
		CreateDevice();
		m_rtvHeap.Initialize(64, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvHeap.Initialize(64, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		m_samplerHeap.Initialize(64, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		m_srvHeap.Initialize(4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_bufferPool.Initialize(4096);
		m_meshPool.Initialize(32);

		CreateCommandQueues();
		CreateSwapChain(window->GetHandle());
		CreateRenderTargets();
		CreateDepthStencil(window->GetWidth(), window->GetHeight());
		CreateCommandAllocators();
		CreateCommandLists();
		CreateFences();
		CreateCamera();
		CreateDefaultMesh();

		InitMainPass();
		InitShadowPass();
		InitImGuiPass();
	}

	void Renderer::ClearGraphicsElements()
	{
		m_batches.clear();
	}

	void Renderer::SubmitGraphicsElement(EntityID id, Handle<Mesh> mesh, const InstanceData& instanceData)
	{
		Assert(id != ID::invalidID);
		Assert(mesh.IsValid());

		if (m_batches.find(mesh.GetID()) == m_batches.end())
		{
			InstancedBatch batch{};
			for (dU32 i = 0; i < ms_frameCount; i++)
			{
				BufferDesc desc{ EBufferUsage::Upload };
				batch.instancesDataBuffer[i] = CreateBuffer(desc, nullptr, gs_instanceDataSize);
				batch.instancesDataViews[i] = m_srvHeap.Allocate();
			}
			m_batches.emplace(mesh.GetID(), batch );
		}

		InstancedBatch& batch{ m_batches[mesh.GetID()] };

		auto it = batch.lookupGraphicsElements.find(id);
		if (it != batch.lookupGraphicsElements.end())
		{
			dU32 index = (*it).second;
			batch.instancesData[index] = instanceData;
			batch.graphicsEntities[index] = id;
		}
		else
		{
			batch.lookupGraphicsElements[id] = (dU32)batch.instancesData.size();
			batch.instancesData.emplace_back(instanceData);
			batch.graphicsEntities.emplace_back(id);
		}
	}

	void Renderer::ClearPointLights()
	{
		m_pointLights.clear();
		m_pointLightEntities.clear();
		m_lookupPointLights.clear();
	}

	void Renderer::RemovePointLight(EntityID id)
	{
		auto it = m_lookupPointLights.find(id);
		if (it == m_lookupPointLights.end())
			return;

		const dU32 index = (*it).second;
		const EntityID entity = m_pointLightEntities[index];

		if (index < m_pointLights.size() - 1)
		{
			m_pointLights[index] = std::move(m_pointLights.back());
			m_pointLightEntities[index] = m_pointLightEntities.back();

			m_lookupPointLights[m_pointLightEntities[index]] = index;
		}

		m_pointLights.pop_back();
		m_pointLightEntities.pop_back();
		m_lookupPointLights.erase(id);
	}

	void Renderer::SubmitPointLight(EntityID id, const PointLight& light)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookupPointLights.find(id);
		if (it != m_lookupPointLights.end())
		{
			dU32 index = (*it).second;
			m_pointLights[index] = light;
			m_pointLightEntities[index] = id;
		}
		else
		{
			m_lookupPointLights[id] = (dU32)m_pointLights.size();
			m_pointLights.emplace_back(light);
			m_pointLightEntities.emplace_back(id);
		}
	}

	void Renderer::ClearDirectionalLights()
	{
		m_directionalLights.clear();
		m_directionalLightEntities.clear();
		m_lookupDirectionalLights.clear();
	}

	void Renderer::RemoveDirectionalLight(EntityID id)
	{
		auto it = m_lookupDirectionalLights.find(id);
		if (it == m_lookupDirectionalLights.end())
			return;

		const dU32 index = (*it).second;
		const EntityID entity = m_directionalLightEntities[index];

		if (index < m_directionalLights.size() - 1)
		{
			m_directionalLights[index] = std::move(m_directionalLights.back());
			m_directionalLightEntities[index] = m_directionalLightEntities.back();

			m_lookupDirectionalLights[m_directionalLightEntities[index]] = index;
		}

		m_directionalLights.pop_back();
		m_directionalLightEntities.pop_back();
		m_lookupDirectionalLights.erase(id);
	}

	void Renderer::SubmitDirectionalLight(EntityID id, const DirectionalLight& light)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookupDirectionalLights.find(id);
		if (it != m_lookupDirectionalLights.end())
		{
			dU32 index = (*it).second;
			m_directionalLights[index] = light;
			m_directionalLightEntities[index] = id;
		}
		else
		{
			m_lookupDirectionalLights[id] = (dU32)m_directionalLights.size();
			m_directionalLights.emplace_back(light);
			m_directionalLightEntities.emplace_back(id);
		}
	}

	void Renderer::CreateCamera()
	{
		BufferDesc camBufferDesc{ EBufferUsage::Upload };
		dU32 size{ sizeof(CameraConstantBuffer) };
		m_cameraMatrixBuffer = CreateBuffer(camBufferDesc, nullptr, size);
	}

	void Renderer::UpdateCamera(const CameraComponent* pCamera, const dVec3& pos)
	{
		if (pCamera)
		{
			// TODO : Camera should be linked to a viewport (TODO : Make viewport)
			// TODO : Get viewport dimensions
			constexpr float aspectRatio = 1600.f / 900.f;
			dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(pCamera->verticalFieldOfView), aspectRatio, 0.1f, 1000.0f) };
			CameraConstantBuffer cameraData
			{
				 pCamera->viewMatrix * projectionMatrix,
				 dVec4{ pos.x, pos.y, pos.z , 0.f }
			};
			UpdateBuffer(m_cameraMatrixBuffer, &cameraData, sizeof(CameraConstantBuffer));
		}
	}

	void Renderer::Render()
	{
		Profile(Render);
		BeginFrame();
		ExecuteShadowPass();
		ExecuteMainPass();
		ExecuteImGuiPass();
		EndFrame();
	}

	void Renderer::RemoveGraphicsElement(EntityID id, Handle<Mesh> meshHandle)
	{
		Assert(m_batches.find(meshHandle.GetID()) != m_batches.end());

		InstancedBatch& batch{ m_batches[meshHandle.GetID()] };

		Assert(batch.lookupGraphicsElements.find(id) != batch.lookupGraphicsElements.end());

		const dU32 index = batch.lookupGraphicsElements[id];
		const EntityID entity = batch.graphicsEntities[index];

		if (index < batch.instancesData.size() - 1)
		{
			batch.instancesData[index] = batch.instancesData.back();
			batch.graphicsEntities[index] = batch.graphicsEntities.back();
			batch.lookupGraphicsElements[batch.graphicsEntities[index]] = index;
		}

		batch.instancesData.pop_back();
		batch.graphicsEntities.pop_back();
		batch.lookupGraphicsElements.erase(id);
	}


	void Renderer::WaitForFrame(const dU64 frameIndex)
	{
		Profile(WaitForFrame);

		Assert(m_fence && m_fenceEvent.IsValid());

		const UINT64 frameFenceValue{ m_fenceValues[frameIndex] };
		if (m_fence->GetCompletedValue() < frameFenceValue)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(frameFenceValue, NULL))
		}

		WaitForCopy();
	}

	void Renderer::WaitForCopy()
	{
		Assert(m_copyFence && m_copyFenceEvent.IsValid());

		m_copyCommandQueue->Signal(m_copyFence.Get(), 1);
		if (m_copyFence->GetCompletedValue() < 1)
		{
			ThrowIfFailed(m_copyFence->SetEventOnCompletion(1, NULL))
		}
		m_copyFence->Signal(0);
	}

	void Renderer::Shutdown()
	{
		m_bIsInitialized = false;

		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			WaitForFrame(i);
		}

		m_commandQueue.Reset();
		m_commandList.Reset();
		m_copyCommandQueue.Reset();
		m_copyCommandAllocator.Reset();
		m_copyCommandList.Reset();

		for (auto& [meshID, batch] : m_batches)
		{
			batch.graphicsEntities.clear();
			batch.instancesData.clear();
			batch.lookupGraphicsElements.clear();

			for (dSizeT i{ 0 }; i < ms_frameCount; i++)
			{
				ReleaseBuffer(batch.instancesDataBuffer[i]);
				m_srvHeap.Free(batch.instancesDataViews[i]);
			}

			m_meshPool.Remove(meshID);
		}

		if (m_meshPool.IsValid(m_cubeMesh))
			m_meshPool.Remove(m_cubeMesh);

		ReleaseBuffer(m_cameraMatrixBuffer);
		for (dSizeT i{ 0 }; i < ms_frameCount; i++)
		{
			m_commandAllocators[i].Reset();
			m_backBuffers[i].Reset();
			m_rtvHeap.Free(m_backBufferViews[i]);
			ReleaseBuffer(m_directionalLightsBuffer[i]);
			ReleaseBuffer(m_pointLightsBuffer[i]);
			m_srvHeap.Free(m_pointLightsViews[i]);
			m_srvHeap.Free(m_directionalLightsViews[i]);
			m_backBuffers[i].Reset();
		}
		m_rtvHeap.Release();

		m_depthStencilBuffer.Reset();
		m_dsvHeap.Free(m_depthBufferView);

		for (dSizeT i{ 0 }; i < ms_shadowMapCount; i++)
		{
			for (dSizeT j{ 0 }; j < ms_frameCount; j++)
			{
				ReleaseBuffer(m_shadowCameraBuffers[i][j]);
			}
			m_shadowMaps->Reset();
			m_dsvHeap.Free(m_shadowDepthViews[i]);
			m_srvHeap.Free(m_shadowResourceViews[i]);
		}
		m_dsvHeap.Release();
		m_srvHeap.Release();

		m_samplerHeap.Free(m_shadowMapSamplerView);
		m_samplerHeap.Release();

		m_fenceEvent.Close();
		m_fence.Reset();
		m_copyFenceEvent.Close();
		m_copyFence.Reset();

		m_rootSignature.Reset();
		m_pipelineState.Reset();

		for (dU64 i{ 0 }; i < ms_frameCount; i++)
		{
			ReleaseDyingResources(i);
		}

		m_bufferPool.Release();
		m_meshPool.Release();

		m_swapChain.Reset();
		m_device.Reset();
		m_factory.Reset();

		m_imguiHeap->Release();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
	}

	void Renderer::OnResize(int x, int y)
	{
		// Wait until all previous frames are processed.
		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			WaitForFrame(i);
		}

		// Release resources that are tied to the swap chain.
		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			m_backBuffers[i].Reset();
		}

		ThrowIfFailed(m_swapChain->ResizeBuffers(
			ms_frameCount,
			0, 0,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		));

		for (UINT i = 0; i < ms_frameCount; i++)
		{
			m_rtvHeap.Free(m_backBufferViews[i]);
		}
		m_dsvHeap.Free(m_depthBufferView);

		CreateRenderTargets();
		CreateDepthStencil(x, y);

		m_viewport.Width = static_cast<float>(x);
		m_viewport.Height = static_cast<float>(y);

		m_scissorRect.right = static_cast<LONG>(x);
		m_scissorRect.bottom = static_cast<LONG>(y);
	}

	Handle<Buffer> Renderer::CreateBuffer(const BufferDesc& desc, const void* pData, dU32 size)
	{
		return m_bufferPool.Create(desc, pData, size);
	}

	void Renderer::UpdateBuffer(Handle<Buffer> handle, const void* pData, dU32 size)
	{
		m_bufferPool.Get(handle).UploadData(pData, size);
	}

	void Renderer::ReleaseBuffer(Handle<Buffer> handle)
	{
		m_bufferPool.Remove(handle);
	}

	Buffer& Renderer::GetBuffer(Handle<Buffer> handle) 
	{
		return m_bufferPool.Get(handle);
	}

	Handle<Mesh> Renderer::CreateMesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices)
	{
		return m_meshPool.Create(indices, vertices);
	}

	void Renderer::ReleaseMesh(Handle<Mesh> handle)
	{
		m_meshPool.Remove(handle);
	}

	Mesh& Renderer::GetMesh(Handle<Mesh> handle)
	{
		return m_meshPool.Get(handle);
	}

	void Renderer::CreateDefaultMesh()
	{
		static const dVector<dU32> defaultCubeIndices
		{
			0, 1, 2, 0, 2, 3,			//Face
			4, 6, 5, 4, 7, 6,			//Back
			10, 11, 9, 10, 9, 8,		//Left
			13, 12, 14, 13, 14, 15,		//Right
			16, 18, 19, 16, 19, 17,		//Top
			22, 20, 21, 22, 21, 23		//Bottom
		};

		static const dVector<Vertex> defaultCubeVertices
		{
			{ {-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f } }, // 0
			{ {-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f } }, // 1
			{ { 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f } }, // 2
			{ { 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f } }, // 3
									 		  
			{ {-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f } }, // 4
			{ {-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f } }, // 5
			{ { 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f } }, // 6
			{ { 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f } }, // 7
									 		  
			{ {-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f } }, // 8
			{ {-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f } }, // 9
			{ {-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f } }, // 10
			{ {-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f } }, // 11
									 		  
			{ { 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f } }, // 12
			{ { 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f } }, // 13
			{ { 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f } }, // 14
			{ { 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f } }, // 15
									 		  
			{ {-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f } }, // 16
			{ { 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f } }, // 17
			{ {-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f } }, // 18
			{ { 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f } }, // 19
									 			    
			{ {-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f } }, // 20
			{ { 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f } }, // 21
			{ {-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f } }, // 22
			{ { 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f } }, // 23
		};

		m_cubeMesh = m_meshPool.Create(defaultCubeIndices, defaultCubeVertices);
	}

	void Renderer::ReleaseResource(IUnknown* resource)
	{
		m_dyingResources[m_frameIndex].emplace_back(resource);
	}

	void Renderer::CreateFactory()
	{
		UINT dxgiFactoryFlags{ 0 };
#if defined(_DEBUG)
		{
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));
	}

	void Renderer::EnableDebugLayer() const
	{
#ifdef _DEBUG
		ID3D12Debug* pDebugInterface{ nullptr };
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugInterface)));
		ID3D12Debug1* pDebugController{ nullptr };
		ThrowIfFailed(pDebugInterface->QueryInterface(IID_PPV_ARGS(&pDebugController)));
		pDebugController->EnableDebugLayer();
		pDebugController->Release();
		pDebugInterface->Release();
#endif
	}

	void Renderer::CreateDevice()
	{
		// Create Adapter
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

		for (UINT adapterIndex{ 0 };
			DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &adapter);
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Don't select the Basic Render Driver adapter.
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			//  Check if the adapter supports Direct3D 12, and use that for the rest
			// of the application
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
				_uuidof(ID3D12Device), nullptr)))
			{
				break;
			}

			// Else we won't use this iteration's adapter, so release it
			adapter->Release();
		}

		// Create Device
		ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_device)));
		NameDXObject(m_device, L"Device");
	}

	void Renderer::CreateCommandQueues()
	{
		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
		NameDXObject(m_commandQueue, L"CommandQueue");

		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		ThrowIfFailed(m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyCommandQueue)));
		NameDXObject(m_copyCommandQueue, L"CopyCommandQueue");
	}

	void Renderer::CreateSwapChain(HWND handle)
	{
		RECT clientRect;
		GetClientRect(handle, &clientRect);

		m_scissorRect.left = 0;
		m_scissorRect.top = 0;
		m_scissorRect.right = static_cast<LONG>(clientRect.right);
		m_scissorRect.bottom = static_cast<LONG>(clientRect.bottom);

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(clientRect.right);
		m_viewport.Height = static_cast<float>(clientRect.bottom);
		m_viewport.MinDepth = 0.f;
		m_viewport.MaxDepth = 1.f;

		DXGI_SWAP_CHAIN_DESC swapChainDesc {};
		swapChainDesc.BufferCount = ms_frameCount;
		swapChainDesc.BufferDesc.Width = 0;
		swapChainDesc.BufferDesc.Height = 0;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = handle;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = TRUE;

		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		ThrowIfFailed(m_factory->CreateSwapChain(
			m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			&swapChainDesc,
			&swapChain
		));
		ThrowIfFailed(m_factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swapChain.As(&m_swapChain));
	}

	void Renderer::CreateRenderTargets()
	{
		// Create frame resources.
		{
			// Create a RTV for each frame.
			for (UINT i = 0; i < ms_frameCount; i++)
			{
				m_backBufferViews[i] = m_rtvHeap.Allocate();
				ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
				m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, m_backBufferViews[i].cpuAdress);
				NameDXObjectIndexed(m_backBuffers[i], i, L"RenderTarget");
			}
		}
	}

	void Renderer::CreateDepthStencil(int width, int height)
	{
		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 0;
		heapProps.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC dsDesc {};
		dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsDesc.Width = width;
		dsDesc.Height = height;
		dsDesc.DepthOrArraySize = 1;
		dsDesc.MipLevels = 1;
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsDesc.Alignment = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&m_depthStencilBuffer)
		));
		NameDXObject(m_depthStencilBuffer, L"DepthStencilBuffer");

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		
		m_depthBufferView = m_dsvHeap.Allocate();
		m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc,
			m_depthBufferView.cpuAdress);
	}

	void Renderer::CreateCommandAllocators()
	{
		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
			NameDXObjectIndexed(m_commandAllocators[i], i, L"CommandAllocators");
		}

		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCommandAllocator)));
		NameDXObject(m_copyCommandAllocator, L"CopyCommandAllocator");
	}

	void Renderer::CreateSamplers()
	{
		// Describe and create the point clamping sampler, which is 
		// used for the shadow map.
		m_shadowMapSamplerView = m_samplerHeap.Allocate();
		D3D12_SAMPLER_DESC clampSamplerDesc {};
		clampSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		clampSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		clampSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		clampSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		clampSamplerDesc.MipLODBias = 0.0f;
		clampSamplerDesc.MaxAnisotropy = 1;
		clampSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		clampSamplerDesc.BorderColor[0] = clampSamplerDesc.BorderColor[1] = clampSamplerDesc.BorderColor[2] = clampSamplerDesc.BorderColor[3] = 0;
		clampSamplerDesc.MinLOD = 0;
		clampSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		m_device->CreateSampler(&clampSamplerDesc, m_shadowMapSamplerView.cpuAdress);
	}

	void Renderer::CreateRootSignature()
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ms_shadowMapCount, 2);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[6];
		// Global constant (Camera matrices)
		rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL);
		// Instances Data
		rootParameters[1].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_VERTEX);
		// Point light buffers
		rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		// Directional light buffers
		rootParameters[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		// Shadow maps
		rootParameters[4].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		// Sampler
		rootParameters[5].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);

		Microsoft::WRL::ComPtr<ID3DBlob> signature{nullptr};
		Microsoft::WRL::ComPtr<ID3DBlob> error{ nullptr };
		if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error)))
		{
			OutputDebugStringA((const char*)error->GetBufferPointer());
			Assert(0);
		}
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	void Renderer::CreatePipeline()
	{
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader{ nullptr };
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader{ nullptr };
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		dWString vertexPath = SHADER_DIR;
		vertexPath.append(L"VertexShader.cso");
		dWString pixelPath = SHADER_DIR;
		pixelPath.append(L"PixelShader.cso");

		ThrowIfFailed(D3DReadFileToBlob(vertexPath.c_str(), &vertexShader));
		ThrowIfFailed(D3DReadFileToBlob(pixelPath.c_str(), &pixelShader));
		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	void Renderer::CreateCommandLists()
	{
		// Create the command list.
		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_commandList->Close());
		NameDXObject(m_commandList, L"CommandList");

		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList)));
		ThrowIfFailed(m_copyCommandList->Close());
		NameDXObject(m_copyCommandList, L"CopyCommandList");
	}

	void Renderer::CreateFences()
	{
		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		NameDXObject(m_fence, L"Fence");

		m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!m_fenceEvent.IsValid())
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
		for (dU32 i{ 0 }; i < ms_frameCount; i++)
		{
			m_fenceValues[i] = 0;
		}

		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence)));
		NameDXObject(m_copyFence, L"CopyFence");

		m_copyFenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!m_copyFenceEvent.IsValid())
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	void Renderer::InitShadowPass()
	{
		CreateSamplers();

		CD3DX12_RESOURCE_DESC shadowTextureDesc(
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			8192,
			8192,
			1,
			1,
			DXGI_FORMAT_D32_FLOAT,
			1,
			0,
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		for (dU32 i = 0; i < ms_shadowMapCount; i++)
		{
				// Create textures
				ThrowIfFailed(m_device->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&shadowTextureDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					&clearValue,
					IID_PPV_ARGS(&m_shadowMaps[i])));

				NameDXObject(m_shadowMaps[i], L"ShadowMapBuffer");

				DescriptorHandle shadowDepthView{ m_dsvHeap.Allocate() };
				D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
				depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
				depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				depthStencilViewDesc.Texture2D.MipSlice = 0;
				m_device->CreateDepthStencilView(m_shadowMaps[i].Get(), &depthStencilViewDesc, shadowDepthView.cpuAdress);
				m_shadowDepthViews[i] = shadowDepthView;

				DescriptorHandle shadowResourceView{ m_srvHeap.Allocate() };
				D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc = {};
				shadowSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
				shadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				shadowSrvDesc.Texture2D.MipLevels = 1;
				shadowSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				m_device->CreateShaderResourceView(m_shadowMaps[i].Get(), &shadowSrvDesc, shadowResourceView.cpuAdress);
				m_shadowResourceViews[i] = shadowResourceView;

				for (dU32 j{ 0 }; j < ms_frameCount; j++)
				{
					BufferDesc desc{ EBufferUsage::Upload };
					m_shadowCameraBuffers[i][j] = CreateBuffer(desc, nullptr, sizeof(CameraConstantBuffer));
				}
		}
	}

	void Renderer::InitMainPass()
	{
		CreateRootSignature();
		CreatePipeline();
		CreatePointLightsBuffer();
		CreateDirectionalLightsBuffer();
	}

	void Renderer::InitImGuiPass()
	{
		//Create ImGui descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imguiHeap)));
		ImGui_ImplDX12_Init(m_device.Get(), ms_frameCount,
			DXGI_FORMAT_R8G8B8A8_UNORM, m_imguiHeap,
			m_imguiHeap->GetCPUDescriptorHandleForHeapStart(),
			m_imguiHeap->GetGPUDescriptorHandleForHeapStart());
	}

	void Renderer::CreatePointLightsBuffer()
	{
		constexpr dU32 pointLightSize = (dU32)sizeof(PointLight);

		for (int i = 0; i < ms_frameCount; i++)
		{
			BufferDesc desc{ EBufferUsage::Upload };
			m_pointLightsBuffer[i] = CreateBuffer(desc, nullptr, pointLightSize);
			m_pointLightsViews[i] = m_srvHeap.Allocate();
		}
	}

	void Renderer::CreateDirectionalLightsBuffer()
	{
		constexpr dU32 directionalLightSize = (dU32)sizeof(DirectionalLight);

		for (int i = 0; i < ms_frameCount; i++)
		{
			BufferDesc desc{ EBufferUsage::Upload };
			m_directionalLightsBuffer[i] = CreateBuffer(desc ,nullptr, directionalLightSize);
			m_directionalLightsViews[i] = m_srvHeap.Allocate();
		}
	}

	void Renderer::UpdateDirectionalLights()
	{
		Profile(UpdateDirectionalLights);
		if (m_directionalLights.empty())
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = 0;
			srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(DirectionalLight));
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_device->CreateShaderResourceView(0, &srvDesc, m_directionalLightsViews[m_frameIndex].cpuAdress);
			return;
		}

		Handle<Buffer>& directionalLightHandle{ m_directionalLightsBuffer[m_frameIndex] };

		if ( ( m_directionalLights.size() * sizeof(DirectionalLight) ) > GetBuffer(directionalLightHandle).GetSize())
		{
			BufferDesc desc{ EBufferUsage::Upload };
			dU32 size{ (dU32)(m_directionalLights.size() * sizeof(DirectionalLight)) };
			ReleaseBuffer(directionalLightHandle);
			directionalLightHandle = CreateBuffer(desc , m_directionalLights.data(), size);
		}
		else
		{
			UpdateBuffer(directionalLightHandle, m_directionalLights.data(), (dU32)(m_directionalLights.size() * sizeof(DirectionalLight)));
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_directionalLights.size());
		srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(DirectionalLight));
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		m_device->CreateShaderResourceView(GetBuffer(directionalLightHandle).GetResource(), &srvDesc, m_directionalLightsViews[m_frameIndex].cpuAdress);
	}

	void Renderer::UpdateInstancesData()
	{
		Profile(UpdateInstancesData);
		for (auto& [meshID, batch] : m_batches)
		{
			if (batch.instancesData.empty())
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
				ZeroMemory(&srvDesc, sizeof(srvDesc));
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = 0;
				srvDesc.Buffer.StructureByteStride = gs_instanceDataSize;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				m_device->CreateShaderResourceView(0, &srvDesc, batch.instancesDataViews[m_frameIndex].cpuAdress);
				return;
			}

			Handle<Buffer>& instancesDataHandle{ batch.instancesDataBuffer[m_frameIndex] };

			if ((batch.instancesData.size() * gs_instanceDataSize) > GetBuffer(instancesDataHandle).GetSize())
			{
				BufferDesc desc{ EBufferUsage::Upload };
				dU32 size{ (dU32)(batch.instancesData.size() * gs_instanceDataSize) };
				ReleaseBuffer(instancesDataHandle);
				instancesDataHandle = CreateBuffer(desc, batch.instancesData.data(), size);
			}
			else
			{
				UpdateBuffer(instancesDataHandle, batch.instancesData.data(), (dU32)(batch.instancesData.size() * gs_instanceDataSize));
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = static_cast<UINT>(batch.instancesData.size());
			srvDesc.Buffer.StructureByteStride = gs_instanceDataSize;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_device->CreateShaderResourceView(GetBuffer(instancesDataHandle).GetResource(), &srvDesc, batch.instancesDataViews[m_frameIndex].cpuAdress);
		}
	}

	void Renderer::ReleaseDyingResources(dU64 frameIndex)
	{
		for (IUnknown* buffer : m_dyingResources[frameIndex])
		{
			buffer->Release();
		}
		m_dyingResources[frameIndex].clear();
	}


	void Renderer::BeginFrame()
	{
		Profile(BeginFrame);
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		WaitForFrame(m_frameIndex);
		ReleaseDyingResources(m_frameIndex);
		ImGui::Render();
		UpdatePointLights();
		UpdateDirectionalLights();
		UpdateInstancesData();

		ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset())
	}

	void Renderer::ExecuteShadowPass()
	{
		Profile(ExecuteShadowPass);

		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

		constexpr D3D12_VIEWPORT viewport{0.f, 0.f, 8192.f, 8192.f, 0.f, 1.f};
		constexpr D3D12_RECT scissorRect{ 0, 0, 8192, 8192};

		m_commandList->RSSetViewports(1, &viewport);
		m_commandList->RSSetScissorRects(1, &scissorRect);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (m_directionalLights.size() > 0)
		{
			UpdateBuffer(m_shadowCameraBuffers[0][m_frameIndex], &m_directionalLights[0].m_viewProj, sizeof(CameraConstantBuffer));

			D3D12_RESOURCE_BARRIER shadowMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMaps[0].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			m_commandList->ResourceBarrier(1, &shadowMapBarrier);

			ID3D12Resource* pCameraMatrixBuffer{ GetBuffer(m_shadowCameraBuffers[0][m_frameIndex]).GetResource() };
			m_commandList->SetGraphicsRootConstantBufferView(0, pCameraMatrixBuffer->GetGPUVirtualAddress());

			m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowDepthViews[0].cpuAdress);    // No render target needed for the shadow pass.
			m_commandList->ClearDepthStencilView(m_shadowDepthViews[0].cpuAdress, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
			m_commandList->SetDescriptorHeaps(1, ppHeaps);

			for (auto& [meshID, batch] : m_batches)
			{
				Profile(Batch);
				if (batch.instancesData.size() > 0)
				{
					m_commandList->SetGraphicsRootDescriptorTable(1, batch.instancesDataViews[m_frameIndex].gpuAdress);

					const Mesh& mesh{ GetMesh(Handle<Mesh>(meshID)) };

					Buffer& vertexBuffer{ GetBuffer(mesh.GeVertexBufferHandle()) };
					Buffer& indexBuffer{ GetBuffer(mesh.GetIndexBufferHandle()) };

					D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
					// Initialize the vertex buffer view.
					vertexBufferView.BufferLocation = vertexBuffer.GetResource()->GetGPUVirtualAddress();
					vertexBufferView.StrideInBytes = sizeof(Vertex);
					vertexBufferView.SizeInBytes = vertexBuffer.GetSize();

					D3D12_INDEX_BUFFER_VIEW indexBufferView;
					indexBufferView.BufferLocation = indexBuffer.GetResource()->GetGPUVirtualAddress();
					indexBufferView.Format = DXGI_FORMAT_R32_UINT;
					indexBufferView.SizeInBytes = indexBuffer.GetSize();

					m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					m_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
					m_commandList->IASetIndexBuffer(&indexBufferView);

					dU32 indexCount{ indexBuffer.GetSize() / (sizeof(dU32)) };
					m_commandList->DrawIndexedInstanced(indexCount, (UINT)batch.instancesData.size(), 0, 0, 0);
				}
			}

			shadowMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMaps[0].Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_commandList->ResourceBarrier(1, &shadowMapBarrier);
		}

		ThrowIfFailed(m_commandList->Close());

		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	void Renderer::ExecuteMainPass()
	{
		Profile(ExecuteMainPass);

		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);

		D3D12_RESOURCE_BARRIER renderTargetBarrier{ CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) };
		m_commandList->ResourceBarrier(1, &renderTargetBarrier);

		m_commandList->OMSetRenderTargets(1, &m_backBufferViews[m_frameIndex].cpuAdress, FALSE, &m_depthBufferView.cpuAdress);

		constexpr float clearColor[] = { 0.05f, 0.05f, 0.075f, 1.0f };
		m_commandList->ClearRenderTargetView(m_backBufferViews[m_frameIndex].cpuAdress, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(m_depthBufferView.cpuAdress, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		ID3D12Resource* pCameraMatrixBuffer{ GetBuffer(m_cameraMatrixBuffer).GetResource() };
		m_commandList->SetGraphicsRootConstantBufferView(0, pCameraMatrixBuffer->GetGPUVirtualAddress());
		ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get(), m_samplerHeap.Get() };
		m_commandList->SetDescriptorHeaps(2, ppHeaps);
		m_commandList->SetGraphicsRootDescriptorTable(2, m_pointLightsViews[m_frameIndex].gpuAdress);
		m_commandList->SetGraphicsRootDescriptorTable(3, m_directionalLightsViews[m_frameIndex].gpuAdress);
		m_commandList->SetGraphicsRootDescriptorTable(4, m_shadowResourceViews[0].gpuAdress);
		m_commandList->SetGraphicsRootDescriptorTable(5, m_shadowMapSamplerView.gpuAdress);

		for (auto& [meshID, batch] : m_batches)
		{
			if (batch.instancesData.size() > 0)
			{
				m_commandList->SetGraphicsRootDescriptorTable(1, batch.instancesDataViews[m_frameIndex].gpuAdress);

				const Mesh& mesh{ GetMesh(Handle<Mesh>(meshID)) };

				Buffer& vertexBuffer{ GetBuffer(mesh.GeVertexBufferHandle()) };
				Buffer& indexBuffer{ GetBuffer(mesh.GetIndexBufferHandle()) };

				D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
				// Initialize the vertex buffer view.
				vertexBufferView.BufferLocation = vertexBuffer.GetResource()->GetGPUVirtualAddress();
				vertexBufferView.StrideInBytes = sizeof(Vertex);
				vertexBufferView.SizeInBytes = vertexBuffer.GetSize();

				D3D12_INDEX_BUFFER_VIEW indexBufferView;
				indexBufferView.BufferLocation = indexBuffer.GetResource()->GetGPUVirtualAddress();
				indexBufferView.Format = DXGI_FORMAT_R32_UINT;
				indexBufferView.SizeInBytes = indexBuffer.GetSize();

				m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				m_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				m_commandList->IASetIndexBuffer(&indexBufferView);

				dU32 indexCount{ indexBuffer.GetSize() / (sizeof(dU32)) };
				m_commandList->DrawIndexedInstanced(indexCount, (UINT)batch.instancesData.size(), 0, 0, 0);
			}
		}

		ThrowIfFailed(m_commandList->Close());

		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	void Renderer::ExecuteImGuiPass()
	{
		Profile(ExecuteImGuiPass);

		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

		m_commandList->OMSetRenderTargets(1, &m_backBufferViews[m_frameIndex].cpuAdress, FALSE, nullptr);
		m_commandList->SetDescriptorHeaps(1, &m_imguiHeap);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

		ThrowIfFailed(m_commandList->Close());

		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	void Renderer::Present()
	{
		Profile(Present);

		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

		D3D12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &renderTargetBarrier);

		ThrowIfFailed(m_commandList->Close());

		ID3D12CommandList* ppCommandLists[] { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		ThrowIfFailed(m_swapChain->Present(1, 0));
	}

	void Renderer::EndFrame()
	{
		Profile(EndFrame);
		Present();
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_frameNumber));
		m_fenceValues[m_frameIndex] = m_frameNumber;
		m_frameNumber++;
	}

	void Renderer::UpdatePointLights()
	{
		Profile(UpdatePointLights);
		if (m_pointLights.empty())
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = 0;
			srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(PointLight));
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_device->CreateShaderResourceView(0, &srvDesc, m_pointLightsViews[m_frameIndex].cpuAdress);
			return;
		}
		
		Handle<Buffer>& pointLightHandle{ m_pointLightsBuffer[m_frameIndex] };

		if (m_pointLights.size() > GetBuffer(pointLightHandle).GetSize() / sizeof(PointLight))
		{
			BufferDesc desc { EBufferUsage::Upload };
			dU32 size = (dU32)(m_pointLights.size() * sizeof(PointLight));
			ReleaseBuffer(pointLightHandle);
			pointLightHandle = CreateBuffer(desc, m_pointLights.data(), size);
		}
		else
		{
			UpdateBuffer(pointLightHandle, m_pointLights.data(), (dU32)(m_pointLights.size() * sizeof(PointLight)));
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_pointLights.size());
		srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(PointLight));
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		m_device->CreateShaderResourceView(GetBuffer(pointLightHandle).GetResource(), &srvDesc, m_pointLightsViews[m_frameIndex].cpuAdress);
	}
}