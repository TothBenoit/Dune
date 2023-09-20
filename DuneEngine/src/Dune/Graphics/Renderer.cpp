#include "pch.h"

#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"
#include "Dune/Graphics/Texture.h"
#include "Dune/Core/Window.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"
#include "Dune/Graphics/Shaders/PointLight.h"
#include "Dune/Graphics/Shaders/DirectionalLight.h"
#include "Dune/Graphics/Shaders/PostProcessGlobals.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"
#include "Dune/Core/JobSystem.h"

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
		m_texturePool.Initialize(64);
		m_meshPool.Initialize(32);
		m_shaderPool.Initialize(16);

		CreateCommandQueues();
		CreateSwapChain(window->GetHandle());
		CreateRenderTargets();
		CreateDepthStencil((dU32)m_viewport.Width, (dU32)m_viewport.Height);
		CreateDefaultShader();
		CreateCommandAllocators();
		CreateCommandLists();
		CreateFences();
		CreateCamera();
		CreateDefaultMesh();

		InitMainPass();
		InitShadowPass();
		InitPostProcessPass();
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
			batch.lookupGraphicsElements[id] = 0;
			batch.instancesData.emplace_back(instanceData);
			batch.graphicsEntities.emplace_back(id);

			BufferDesc desc{ L"InstanceDatasBuffer", gs_instanceDataSize, EBufferUsage::Structured, EBufferMemory::CPU, nullptr, gs_instanceDataSize };
			batch.instancesDataBuffer = CreateBuffer(desc);

			m_batches.emplace(mesh.GetID(), std::move(batch) );
			return;
		}

		InstancedBatch& batch{ m_batches[mesh.GetID()] };
		auto it{ batch.lookupGraphicsElements.find(id) };
		if (it != batch.lookupGraphicsElements.end())
		{
			dU32 index{ (*it).second };
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

	void Renderer::RemoveGraphicsElement(EntityID id, Handle<Mesh> meshHandle)
	{
		Assert(m_batches.find(meshHandle.GetID()) != m_batches.end());

		dHashMap<ID::IDType, InstancedBatch>::iterator it{ m_batches.find(meshHandle.GetID()) };

		InstancedBatch& batch{ it->second };

		Assert(batch.lookupGraphicsElements.find(id) != batch.lookupGraphicsElements.end());

		const dU32 index{ batch.lookupGraphicsElements[id] };
		const EntityID entity{ batch.graphicsEntities[index] };

		if (index < batch.instancesData.size() - 1)
		{
			batch.instancesData[index] = batch.instancesData.back();
			batch.graphicsEntities[index] = batch.graphicsEntities.back();
			batch.lookupGraphicsElements[batch.graphicsEntities[index]] = index;
		}

		batch.instancesData.pop_back();
		batch.graphicsEntities.pop_back();
		batch.lookupGraphicsElements.erase(id);

		if (batch.instancesData.empty())
		{
			ReleaseBuffer(batch.instancesDataBuffer);
			m_batches.erase(it);
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

	void Renderer::SubmitPointLight(EntityID id, const dVec3& color, float intensity, const dVec3& pos, float radius)
	{
		Assert(id != ID::invalidID);

		PointLight light;
		light.m_color = color;
		light.m_intensity = intensity;
		light.m_pos = pos;
		light.m_radius = radius;

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

	void Renderer::SubmitDirectionalLight(EntityID id, const dVec3& color, const dVec3& direction, float intensity, const dMatrix4x4& viewProj)
	{
		Assert(id != ID::invalidID);

		DirectionalLight light;
		light.m_color = color;
		light.m_dir = direction;
		light.m_intensity = intensity;
		light.m_viewProj = viewProj;

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
		BufferDesc camBufferDesc{ L"CameraConstantBuffer", sizeof(CameraConstantBuffer), EBufferUsage::Constant, EBufferMemory::CPU, nullptr};
		m_cameraMatrixBuffer = CreateBuffer(camBufferDesc);
	}

	void Renderer::UpdateCamera(const CameraComponent* pCamera, const dVec3& pos)
	{
		Assert(pCamera);
		m_FOV = pCamera->verticalFieldOfView;
		m_viewMatrix = pCamera->viewMatrix;
		m_cameraPosition = pos;
		m_needCameraUpdate = true;
	}

	void Renderer::Render()
	{
		ProfileFunc();
		BeginFrame();
		Job::JobBuilder jobBuilder;
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { ExecuteShadowPass(m_commandLists[0]);		});
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { ExecuteMainPass(m_commandLists[1]);		});
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { ExecutePostProcessPass(m_commandLists[2]); });
		jobBuilder.DispatchJob<Job::Fence::None>([&]() { ExecuteImGuiPass(m_commandLists[3]);		});
		Job::WaitForCounter(jobBuilder.ExtractWaitCounter());
		EndFrame();
	}

	void Renderer::WaitForFrame(const dU64 frameIndex)
	{
		ProfileFunc();

		Assert(m_fence && m_fenceEvent.IsValid());

		const dU64 frameFenceValue{ m_fenceValues[frameIndex] };
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

	void Renderer::Resize()
	{
		Assert(m_needResize);
		m_needResize = false;

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
		ReleaseTexture(m_intermediateRenderTarget);
		ReleaseTexture(m_depthStencilBuffer);

		ThrowIfFailed(m_swapChain->ResizeBuffers(
			ms_frameCount,
			0, 0,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		));

		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			m_rtvHeap.Free(m_backBufferViews[i]);
		}

		CreateRenderTargets();
		CreateDepthStencil((dU32)m_viewport.Width, (dU32)m_viewport.Height);

		m_needCameraUpdate = true;

		m_intermediateRenderTarget = CreateTexture(
			{
				.debugName = L"IntermediateRenderTarget",
				.usage = ETextureUsage::RTV,
				.dimensions = { (dU32)m_viewport.Width, (dU32)m_viewport.Height, 1 },
				.format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.state = D3D12_RESOURCE_STATE_GENERIC_READ,
				.clearValue = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color{ .05f, 0.05f, 0.075f, 1.0f } }
			}
		);
	}

	void Renderer::Shutdown()
	{
		m_bIsInitialized = false;

		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			WaitForFrame(i);

			for (ID3D12CommandAllocator* pCommandAllocator : m_commandAllocators[i])
			{
				pCommandAllocator->Release();
			}
		}

		for (ID3D12CommandList* pCommandList : m_commandLists)
		{
			pCommandList->Release();
		}

		m_commandQueue.Reset();

		m_copyCommandAllocator.Reset();
		m_copyCommandList.Reset();
		m_copyCommandQueue.Reset();

		for (auto& [meshID, batch] : m_batches)
		{
			batch.graphicsEntities.clear();
			batch.instancesData.clear();
			batch.lookupGraphicsElements.clear();
			ReleaseBuffer(batch.instancesDataBuffer);
			m_meshPool.Remove(meshID);
		}

		if (m_meshPool.IsValid(m_defaultMesh))
			m_meshPool.Remove(m_defaultMesh);

		ReleaseBuffer(m_cameraMatrixBuffer);
		ReleaseBuffer(m_fullScreenIndices);
		ReleaseBuffer(m_postProcessGlobals);

		if (m_directionalLightsBuffer.IsValid())
			ReleaseBuffer(m_directionalLightsBuffer);

		if (m_pointLightsBuffer.IsValid())
			ReleaseBuffer(m_pointLightsBuffer);

		ReleaseTexture(m_intermediateRenderTarget);
		
		for (dU32 i{ 0 }; i < ms_frameCount; i++)
		{
			m_rtvHeap.Free(m_backBufferViews[i]);
			m_backBuffers[i].Reset();
		}

		ReleaseTexture(m_depthStencilBuffer);

		for (dU32 i{ 0 }; i < ms_shadowMapCount; i++)
		{
			ReleaseBuffer(m_shadowCameraBuffers[i]);
			m_dsvHeap.Free(m_shadowMapsDepthViews[i]);
		}
		m_shadowMaps.Reset();
		m_srvHeap.Free(m_shadowMapsResourceView);
		m_srvHeap.Free(m_imguiDescriptorHandle);

		m_samplerHeap.Free(m_shadowMapsSamplerView);

		ReleaseShader(m_depthWriteShader);
		ReleaseShader(m_defaultShader);
		ReleaseShader(m_postProcessShader);

		m_fenceEvent.Close();
		m_fence.Reset();
		m_copyFenceEvent.Close();
		m_copyFence.Reset();

		for (dU64 i{ 0 }; i < ms_frameCount; i++)
		{
			ReleaseDyingResources(i);
		}

		m_rtvHeap.Release();
		m_dsvHeap.Release();
		m_srvHeap.Release();
		m_samplerHeap.Release();

		m_bufferPool.Release();
		m_texturePool.Release();
		m_meshPool.Release();
		m_shaderPool.Release();

		m_swapChain.Reset();
		m_device.Reset();
		m_factory.Reset();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
	}

	void Renderer::OnResize(int x, int y)
	{
		m_viewport.Width = static_cast<float>(x);
		m_viewport.Height = static_cast<float>(y);

		m_scissorRect.right = static_cast<LONG>(x);
		m_scissorRect.bottom = static_cast<LONG>(y);

		// We do not resize immediately because OnResize can be called multiple times in during the frame (due to windows message)
		m_needResize = true;
	}

	Handle<Buffer> Renderer::CreateBuffer(const BufferDesc& desc)
	{
		ProfileFunc();
		return m_bufferPool.Create(desc);
	}

	void Renderer::UploadBuffer(Handle<Buffer> handle, const void* pData, dU32 size)
	{
		ProfileFunc();
		m_bufferPool.Get(handle).UploadData(pData, size);
	}

	void Renderer::MapBuffer(Handle<Buffer> handle, const void* pData, dU32 size)
	{
		ProfileFunc();
		m_bufferPool.Get(handle).MapData(pData, size);
	}

	void Renderer::ReleaseBuffer(Handle<Buffer> handle)
	{
		ProfileFunc();
		m_bufferPool.Remove(handle);
	}

	Buffer& Renderer::GetBuffer(Handle<Buffer> handle)
	{
		return m_bufferPool.Get(handle);
	}

	Handle<Texture> Renderer::CreateTexture(const TextureDesc& desc)
	{
		ProfileFunc();
		return m_texturePool.Create(desc);
	}

	void Renderer::ReleaseTexture(Handle<Texture> handle)
	{
		ProfileFunc();
		m_texturePool.Remove(handle);
	}

	Texture& Renderer::GetTexture(Handle<Texture> handle)
	{
		return m_texturePool.Get(handle);
	}

	Handle<Mesh> Renderer::CreateMesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices)
	{
		ProfileFunc();
		return m_meshPool.Create(indices, vertices);
	}

	void Renderer::ReleaseMesh(Handle<Mesh> handle)
	{
		ProfileFunc();
		m_meshPool.Remove(handle);
	}

	Mesh& Renderer::GetMesh(Handle<Mesh> handle)
	{
		return m_meshPool.Get(handle);
	}

	Shader& Renderer::GetShader(Handle<Shader> handle)
	{
		return m_shaderPool.Get(handle);
	}

	Handle<Shader> Renderer::CreateShader(const ShaderDesc& desc)
	{
		ProfileFunc();
		return m_shaderPool.Create(desc);
	}

	void Renderer::ReleaseShader(Handle<Shader> handle)
	{
		ProfileFunc();
		m_shaderPool.Remove(handle);
	}

	void Renderer::CreateDefaultMesh()
	{
		static const dVector<dU32> defaultMeshIndices
		{
			0, 1, 2, 0, 2, 3,			//Face
			4, 6, 5, 4, 7, 6,			//Back
			10, 11, 9, 10, 9, 8,		//Left
			13, 12, 14, 13, 14, 15,		//Right
			16, 18, 19, 16, 19, 17,		//Top
			22, 20, 21, 22, 21, 23		//Bottom
		};

		static const dVector<Vertex> defaultMeshVertices
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

		m_defaultMesh = m_meshPool.Create(defaultMeshIndices, defaultMeshVertices);
	}

	void Renderer::ReleaseResource(IUnknown* resource)
	{
		ProfileFunc();
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
		m_scissorRect.right = clientRect.right;
		m_scissorRect.bottom = clientRect.bottom;

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
		for (UINT i = 0; i < ms_frameCount; i++)
		{
			m_backBufferViews[i] = m_rtvHeap.Allocate();
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
			m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, m_backBufferViews[i].cpuAdress);
			NameDXObjectIndexed(m_backBuffers[i], i, L"BackBuffers");
		}
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	}

	void Renderer::CreateDepthStencil(dU32 width, dU32 height)
	{
		m_depthStencilBuffer = CreateTexture(
			{
				.debugName = L"DepthStencilBuffer",
				.usage = ETextureUsage::DSV,
				.dimensions = { width, height, 1 },
				.format = DXGI_FORMAT_D16_UNORM,
				.state = D3D12_RESOURCE_STATE_DEPTH_READ,
				.clearValue = {.Format = DXGI_FORMAT_D16_UNORM, .DepthStencil { 1.0f, 0 } }
			}
		);
	}

	void Renderer::CreateCommandAllocators()
	{
		for (dU32 i = 0; i < ms_frameCount; i++)
		{
			m_commandAllocators[i].reserve(m_renderPassCount);
			for (dU32 j = 0; j < m_renderPassCount; j++)
			{
				ID3D12CommandAllocator* pCommandAllocator{ nullptr };
				ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator)));
				NameDXObjectIndexed(pCommandAllocator, i + m_renderPassCount * j, L"CommandAllocator");
				m_commandAllocators[i].push_back(pCommandAllocator);
			}
		}

		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCommandAllocator)));
		NameDXObject(m_copyCommandAllocator, L"CopyCommandAllocator");
	}

	void Renderer::CreateSamplers()
	{
		// Describe and create the point clamping sampler, which is 
		// used for the shadow map.
		m_shadowMapsSamplerView = m_samplerHeap.Allocate();
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
		m_device->CreateSampler(&clampSamplerDesc, m_shadowMapsSamplerView.cpuAdress);
	}

	void Renderer::CreateCommandLists()
	{
		m_commandLists.reserve(m_renderPassCount);
		for (dU32 i = 0; i < m_renderPassCount; i++)
		{
			ID3D12GraphicsCommandList* pCommandList{ nullptr };
			ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex][i], nullptr, IID_PPV_ARGS(&pCommandList)));
			ThrowIfFailed(pCommandList->Close());
			NameDXObjectIndexed(pCommandList, i, L"CommandList");
			m_commandLists.push_back(pCommandList);
		}

		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList)));
		ThrowIfFailed(m_copyCommandList->Close());
		NameDXObject(m_copyCommandList, L"CopyCommandList");
	}

	void Renderer::CreateFences()
	{
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
		CreateDepthWriteShader();

		CD3DX12_RESOURCE_DESC shadowTextureDesc(
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			8192,
			8192,
			ms_shadowMapCount,
			1,
			DXGI_FORMAT_D16_UNORM,
			1,
			0,
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
		clearValue.Format = DXGI_FORMAT_D16_UNORM;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		// Create textures
		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&shadowTextureDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&m_shadowMaps)));
		NameDXObject(m_shadowMaps, L"ShadowMapsBuffer");

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D16_UNORM;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		depthStencilViewDesc.Texture2DArray.MipSlice = 0;
		depthStencilViewDesc.Texture2DArray.ArraySize = 1;

		for (dU32 i{ 0 }; i < ms_shadowMapCount; i++)
		{
			depthStencilViewDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
			DescriptorHandle shadowDepthView{ m_dsvHeap.Allocate() };
			m_device->CreateDepthStencilView(m_shadowMaps.Get(), &depthStencilViewDesc, shadowDepthView.cpuAdress);
			m_shadowMapsDepthViews[i] = shadowDepthView;
		}

		DescriptorHandle shadowResourceView{ m_srvHeap.Allocate() };
		D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc = {};
		shadowSrvDesc.Format = DXGI_FORMAT_R16_UNORM;
		shadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		shadowSrvDesc.Texture2DArray.MipLevels = 1;
		shadowSrvDesc.Texture2DArray.ArraySize = ms_shadowMapCount;
		shadowSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		m_device->CreateShaderResourceView(m_shadowMaps.Get(), &shadowSrvDesc, shadowResourceView.cpuAdress);
		m_shadowMapsResourceView = shadowResourceView;

		for (dU32 i{ 0 }; i < ms_shadowMapCount; i++)
		{
			BufferDesc desc{ L"ShadowMapCameraBuffer", sizeof(dMatrix), EBufferUsage::Constant, EBufferMemory::CPU, nullptr };
			m_shadowCameraBuffers[i] = CreateBuffer(desc);
		}
	}

	void Renderer::InitMainPass()
	{
		m_intermediateRenderTarget = CreateTexture(
			{ 
				.debugName = L"IntermediateRenderTarget",
				.usage = ETextureUsage::RTV,
				.dimensions = { (dU32)m_viewport.Width, (dU32)m_viewport.Height, 1 },
				.format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.state = D3D12_RESOURCE_STATE_GENERIC_READ,
				.clearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color{ .05f, 0.05f, 0.075f, 1.0f } }
			}
		);
	}

	void Renderer::InitPostProcessPass()
	{
		CreatePostProcessShader();

		dU32 indices[3]{ 0, 1, 2 };
		dU32 size{ 3 * sizeof(dU32) };
		BufferDesc desc{ L"FullScreenIndexBuffer", size, EBufferUsage::Index, EBufferMemory::GPUStatic, indices };
		m_fullScreenIndices = Renderer::GetInstance().CreateBuffer(
			{ 
				.debugName	= L"FullScreenIndexBuffer", 
				.byteSize	= size, 
				.usage		= EBufferUsage::Index, 
				.memory		= EBufferMemory::GPUStatic, 
				.pData		= indices 
			}
		);
		Assert(m_fullScreenIndices.IsValid());

		m_postProcessGlobals = Renderer::GetInstance().CreateBuffer(
			{
				.debugName	= L"PostProcessConstants",
				.byteSize	= sizeof(PostProcessGlobals),
				.usage		= EBufferUsage::Constant,
				.memory		= EBufferMemory::CPU,
			}
		);
		Assert(m_postProcessGlobals.IsValid());
	}

	void Renderer::InitImGuiPass()
	{
		m_imguiDescriptorHandle = m_srvHeap.Allocate();
		ImGui_ImplDX12_Init(m_device.Get(), ms_frameCount,
			DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(),
			m_imguiDescriptorHandle.cpuAdress,
			m_imguiDescriptorHandle.gpuAdress);
	}

	void Renderer::UpdateDirectionalLights()
	{
		ProfileFunc();

		Handle<Buffer>& directionalLightHandle{ m_directionalLightsBuffer };

		if (m_directionalLights.empty())
		{
			if (directionalLightHandle.IsValid())
			{
				ReleaseBuffer(directionalLightHandle);
				directionalLightHandle = ID::invalidID;
			}
			return;
		}

		if ( !directionalLightHandle.IsValid() || ( m_directionalLights.size() * sizeof(DirectionalLight) ) > GetBuffer(directionalLightHandle).GetSize())
		{
			if (directionalLightHandle.IsValid())
				ReleaseBuffer(directionalLightHandle);
			
			dU32 size{ (dU32)(m_directionalLights.size() * sizeof(DirectionalLight)) };
			BufferDesc desc{ L"DirectionalLightsBuffer", size, EBufferUsage::Structured, EBufferMemory::CPU, m_directionalLights.data(), sizeof(DirectionalLight) };
			directionalLightHandle = CreateBuffer(desc);
		}
		else
		{
			MapBuffer(directionalLightHandle, m_directionalLights.data(), (dU32)(m_directionalLights.size() * sizeof(DirectionalLight)));
		}
	}

	void Renderer::UpdateInstancesData()
	{
		ProfileFunc();
		for (auto& [meshID, batch] : m_batches)
		{
			Assert(!batch.instancesData.empty());
			
			Handle<Buffer>& instancesDataHandle{ batch.instancesDataBuffer };

			if ((batch.instancesData.size() * gs_instanceDataSize) > GetBuffer(instancesDataHandle).GetSize())
			{
				ReleaseBuffer(instancesDataHandle);
				dU32 size{ (dU32)(batch.instancesData.size() * gs_instanceDataSize) };
				BufferDesc desc{ L"InstanceDatasBuffer", size, EBufferUsage::Structured, EBufferMemory::CPU , batch.instancesData.data(), gs_instanceDataSize };
				instancesDataHandle = CreateBuffer(desc);
			}
			else
			{
				MapBuffer(instancesDataHandle, batch.instancesData.data(), (dU32)(batch.instancesData.size() * gs_instanceDataSize));
			}
		}
	}

	void Renderer::UpdateCameraBuffer()
	{
		if (!m_needCameraUpdate)
			return;

		m_needCameraUpdate = false;

		dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_FOV), m_viewport.Width / m_viewport.Height, m_nearPlane, m_farPlane) };
		CameraConstantBuffer cameraData
		{
			 m_viewMatrix * projectionMatrix,
			 dVec4{ m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z , 1.f }
		};
		MapBuffer(m_cameraMatrixBuffer, &cameraData, sizeof(CameraConstantBuffer));

		dMatrix invProj = DirectX::XMMatrixInverse(nullptr, projectionMatrix);
		PostProcessGlobals globals
		{
			.m_invProj = invProj,
			.m_screenResolution = dVec2(m_viewport.Width, m_viewport.Height),
		};
		MapBuffer(m_postProcessGlobals, &globals, sizeof(PostProcessGlobals));
	}

	void Renderer::CreateDefaultShader()
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
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

		ShaderDesc desc
		{
			.debugName = "DefaultShader",
			.VS = {.fileName = L"Default.hlsl", .entryFunc = L"VSMain" },
			.PS = {.fileName = L"Default.hlsl", .entryFunc = L"PSMain" },
			.rootSignatureDesc = rootSignatureDesc,
			.depthStencilDesc = {.depthEnable = true },
		};
		m_defaultShader = m_shaderPool.Create(desc);
	}

	void Renderer::CreatePostProcessShader()
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[2].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		CD3DX12_STATIC_SAMPLER_DESC1 staticSamplerDesc;
		staticSamplerDesc.Init(0);
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, (D3D12_STATIC_SAMPLER_DESC*)&staticSamplerDesc,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);

		ShaderDesc desc
		{
			.debugName = "PostProcessShader",
			.VS = {.fileName = L"PostProcess.hlsl", .entryFunc = L"VSMain" },
			.PS = {.fileName = L"PostProcess.hlsl", .entryFunc = L"PSMain" },
			.rootSignatureDesc = rootSignatureDesc,
		};
		m_postProcessShader = m_shaderPool.Create(desc);
	}

	void Renderer::CreateDepthWriteShader()
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		// ViewProj matrix
		rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
		// Instances Data
		rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
		);

		ShaderDesc desc
		{
			.debugName = "DepthWriteShader",
			.VS = {.fileName = L"DepthWrite.hlsl", .entryFunc = L"VSMain" },
			.rootSignatureDesc = rootSignatureDesc,
			.depthStencilDesc = { .depthEnable = true },
		};
		m_depthWriteShader = m_shaderPool.Create(desc);
	}

	void Renderer::ReleaseDyingResources(dU64 frameIndex)
	{
		m_dsvHeap.ReleaseDying(frameIndex);
		m_rtvHeap.ReleaseDying(frameIndex);
		m_samplerHeap.ReleaseDying(frameIndex);
		m_srvHeap.ReleaseDying(frameIndex);
		
		for (IUnknown* buffer : m_dyingResources[frameIndex])
		{
			buffer->Release();
		}

		m_dyingResources[frameIndex].clear();
	}


	void Renderer::BeginFrame()
	{
		ProfileFunc();
		WaitForFrame(m_frameIndex);
		ReleaseDyingResources(m_frameIndex);
		if (m_needResize)
			Resize();
		ImGui::Render();
		UpdateCameraBuffer();
		UpdatePointLights();
		UpdateDirectionalLights();
		UpdateInstancesData();

		ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get(), m_samplerHeap.Get() };
		for (dU32 i = 0; i < m_renderPassCount; i++)
		{
			ThrowIfFailed(m_commandAllocators[m_frameIndex][i]->Reset());
			ThrowIfFailed(m_commandLists[i]->Reset(m_commandAllocators[m_frameIndex][i], nullptr));
			m_commandLists[i]->SetDescriptorHeaps(2, ppHeaps);
		}
	}

	void Renderer::ExecuteShadowPass(ID3D12GraphicsCommandList* pCommandList)
	{
		ProfileFunc();

		if (m_directionalLights.empty())
			return;

		Shader& shader = m_shaderPool.Get(m_depthWriteShader);
		pCommandList->SetPipelineState(shader.GetPSO());
		pCommandList->SetGraphicsRootSignature(shader.GetRootSignature());

		constexpr D3D12_VIEWPORT viewport{0.f, 0.f, 8192.f, 8192.f, 0.f, 1.f};
		constexpr D3D12_RECT scissorRect{ 0, 0, 8192, 8192};

		pCommandList->RSSetViewports(1, &viewport);
		pCommandList->RSSetScissorRects(1, &scissorRect);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_RESOURCE_BARRIER shadowMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMaps.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pCommandList->ResourceBarrier(1, &shadowMapBarrier);

		for (dU32 i{ 0 }; i < ms_shadowMapCount && i < m_directionalLights.size() ; i++)
		{
			MapBuffer(m_shadowCameraBuffers[i], &m_directionalLights[i].m_viewProj, sizeof(dMatrix));
			Buffer& cameraMatrixBuffer{ GetBuffer(m_shadowCameraBuffers[i]) };
			
			pCommandList->SetGraphicsRootConstantBufferView(0, cameraMatrixBuffer.GetGPUAdress() );
			pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowMapsDepthViews[i].cpuAdress);    // No render target needed for the shadow pass.
			pCommandList->ClearDepthStencilView(m_shadowMapsDepthViews[i].cpuAdress, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			for (const auto& [meshID, batch] : m_batches)
			{
				Profile("Batch");
				if (batch.instancesData.size() > 0)
				{
					Buffer& instanceBuffer{ GetBuffer(batch.instancesDataBuffer)};
					pCommandList->SetGraphicsRootDescriptorTable(1, instanceBuffer.GetView().gpuAdress);

					const Mesh& mesh{ GetMesh(Handle<Mesh>(meshID)) };

					Buffer& vertexBuffer{ GetBuffer(mesh.GeVertexBufferHandle()) };
					Buffer& indexBuffer{ GetBuffer(mesh.GetIndexBufferHandle()) };

					D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
					// Initialize the vertex buffer view.
					vertexBufferView.BufferLocation = vertexBuffer.GetGPUAdress();
					vertexBufferView.StrideInBytes = sizeof(Vertex);
					vertexBufferView.SizeInBytes = vertexBuffer.GetSize();

					D3D12_INDEX_BUFFER_VIEW indexBufferView;
					indexBufferView.BufferLocation = indexBuffer.GetGPUAdress();
					indexBufferView.Format = DXGI_FORMAT_R32_UINT;
					indexBufferView.SizeInBytes = indexBuffer.GetSize();

					pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
					pCommandList->IASetIndexBuffer(&indexBufferView);

					dU32 indexCount{ indexBuffer.GetSize() / (sizeof(dU32)) };
					pCommandList->DrawIndexedInstanced(indexCount, (UINT)batch.instancesData.size(), 0, 0, 0);
				}
			}
		}

		shadowMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMaps.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		pCommandList->ResourceBarrier(1, &shadowMapBarrier);
	}

	void Renderer::ExecuteMainPass(ID3D12GraphicsCommandList* pCommandList)
	{
		ProfileFunc();
	
		Shader& shader = m_shaderPool.Get(m_defaultShader);
		pCommandList->SetPipelineState(shader.GetPSO());
		pCommandList->SetGraphicsRootSignature(shader.GetRootSignature());
		pCommandList->RSSetViewports(1, &m_viewport);
		pCommandList->RSSetScissorRects(1, &m_scissorRect);

		Texture& intermediateRenderTarget{ GetTexture(m_intermediateRenderTarget) };
		Texture& depthStencilBuffer { GetTexture(m_depthStencilBuffer) };

		D3D12_RESOURCE_BARRIER barriers[2]
		{
			CD3DX12_RESOURCE_BARRIER::Transition(intermediateRenderTarget.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE),
		};
		pCommandList->ResourceBarrier(2, barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE intermediateRenderTargetCPUadress = intermediateRenderTarget.GetRTV().cpuAdress;
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilBufferCPUadress = depthStencilBuffer.GetDSV().cpuAdress;

		pCommandList->OMSetRenderTargets(1, &intermediateRenderTargetCPUadress, FALSE, &depthStencilBufferCPUadress);

		constexpr float clearColor[] = { 0.05f, 0.05f, 0.075f, 1.0f };
		pCommandList->ClearRenderTargetView(intermediateRenderTarget.GetRTV().cpuAdress, clearColor, 0, nullptr);
		pCommandList->ClearDepthStencilView(depthStencilBuffer.GetDSV().cpuAdress, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		Buffer& cameraMatrixBuffer{ GetBuffer(m_cameraMatrixBuffer)};
		pCommandList->SetGraphicsRootConstantBufferView(0, cameraMatrixBuffer.GetGPUAdress());

		if (!m_pointLights.empty())
		{
			Buffer& buffer{ GetBuffer(m_pointLightsBuffer) };
			pCommandList->SetGraphicsRootDescriptorTable(2, buffer.GetView().gpuAdress);
		}
		if (!m_directionalLights.empty())
		{
			Buffer& buffer{ GetBuffer(m_directionalLightsBuffer) };
			pCommandList->SetGraphicsRootDescriptorTable(3, buffer.GetView().gpuAdress);
		}
		pCommandList->SetGraphicsRootDescriptorTable(4, m_shadowMapsResourceView.gpuAdress);
		pCommandList->SetGraphicsRootDescriptorTable(5, m_shadowMapsSamplerView.gpuAdress);

		for (const auto& [meshID, batch] : m_batches)
		{
			Profile("Batch");

			Assert(batch.instancesData.size() > 0);

			Buffer& instanceBuffer{ GetBuffer(batch.instancesDataBuffer) };
			pCommandList->SetGraphicsRootDescriptorTable(1, instanceBuffer.GetView().gpuAdress);

			const Mesh& mesh{ GetMesh(Handle<Mesh>(meshID)) };

			Buffer& vertexBuffer{ GetBuffer(mesh.GeVertexBufferHandle()) };
			Buffer& indexBuffer{ GetBuffer(mesh.GetIndexBufferHandle()) };

			D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
			// Initialize the vertex buffer view.
			vertexBufferView.BufferLocation = vertexBuffer.GetGPUAdress();
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			vertexBufferView.SizeInBytes = vertexBuffer.GetSize();

			D3D12_INDEX_BUFFER_VIEW indexBufferView;
			indexBufferView.BufferLocation = indexBuffer.GetGPUAdress();
			indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			indexBufferView.SizeInBytes = indexBuffer.GetSize();

			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			pCommandList->IASetIndexBuffer(&indexBufferView);

			dU32 indexCount{ indexBuffer.GetSize() / (sizeof(dU32)) };
			pCommandList->DrawIndexedInstanced(indexCount, (UINT)batch.instancesData.size(), 0, 0, 0);
		}
	}

	void Renderer::ExecuteImGuiPass(ID3D12GraphicsCommandList* pCommandList)
	{
		ProfileFunc();

		pCommandList->OMSetRenderTargets(1, &m_backBufferViews[m_frameIndex].cpuAdress, FALSE, nullptr);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
	}

	void Renderer::ExecutePostProcessPass(ID3D12GraphicsCommandList* pCommandList)
	{
		ProfileFunc();
		Shader& shader = m_shaderPool.Get(m_postProcessShader);
		pCommandList->SetPipelineState(shader.GetPSO());
		pCommandList->SetGraphicsRootSignature(shader.GetRootSignature());
		pCommandList->RSSetViewports(1, &m_viewport);
		pCommandList->RSSetScissorRects(1, &m_scissorRect);

		Texture& intermediateRenderTarget{ GetTexture(m_intermediateRenderTarget) };
		Texture& depthStencilBuffer{ GetTexture(m_depthStencilBuffer) };

		D3D12_RESOURCE_BARRIER barriers[3]
		{
			CD3DX12_RESOURCE_BARRIER::Transition(intermediateRenderTarget.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ),
			CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ),
		};

		pCommandList->ResourceBarrier(3, barriers);

		pCommandList->OMSetRenderTargets(1, &m_backBufferViews[m_frameIndex].cpuAdress, false, nullptr);

		pCommandList->SetGraphicsRootDescriptorTable(0, intermediateRenderTarget.GetSRV().gpuAdress);
		pCommandList->SetGraphicsRootDescriptorTable(1, depthStencilBuffer.GetSRV().gpuAdress);
		pCommandList->SetGraphicsRootConstantBufferView(2, GetBuffer(m_postProcessGlobals).GetGPUAdress());
		
		Buffer& indexBuffer{ GetBuffer(m_fullScreenIndices) };

		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		indexBufferView.BufferLocation = indexBuffer.GetGPUAdress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = indexBuffer.GetSize();

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->IASetIndexBuffer(&indexBufferView);
		pCommandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
	}

	void Renderer::Present()
	{
		ProfileFunc();

		D3D12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandLists.back()->ResourceBarrier(1, &renderTargetBarrier);

		for (ID3D12GraphicsCommandList* pCommandList : m_commandLists)
		{
			pCommandList->Close();
		}

		m_commandQueue->ExecuteCommandLists((dU32)m_commandLists.size(), (ID3D12CommandList**)m_commandLists.data());

		ThrowIfFailed(m_swapChain->Present(1, 0));
	}

	void Renderer::EndFrame()
	{
		ProfileFunc();
		Present();
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_elapsedFrame));
		m_fenceValues[m_frameIndex] = m_elapsedFrame;
		m_elapsedFrame++;
		m_frameIndex = (m_frameIndex + 1) % ms_frameCount;
	}

	void Renderer::UpdatePointLights()
	{
		ProfileFunc();
		Handle<Buffer>& pointLightHandle{ m_pointLightsBuffer };
		
		if (m_pointLights.empty())
		{
			if (pointLightHandle.IsValid())
			{
				ReleaseBuffer(pointLightHandle);
				pointLightHandle = ID::invalidID;
			}

			return;
		}

		if ( !pointLightHandle.IsValid() || m_pointLights.size() > GetBuffer(pointLightHandle).GetSize() / sizeof(PointLight))
		{
			if (pointLightHandle.IsValid())
				ReleaseBuffer(pointLightHandle);
			
			dU32 size{ (dU32)(m_pointLights.size() * sizeof(PointLight)) };
			BufferDesc desc { L"PointLightsBuffer", size, EBufferUsage::Structured, EBufferMemory::CPU, m_pointLights.data(), sizeof(PointLight)};
			pointLightHandle = CreateBuffer(desc);
		}
		else
		{
			MapBuffer(pointLightHandle, m_pointLights.data(), (dU32)(m_pointLights.size() * sizeof(PointLight)));
		}
	}
}