#pragma once

#include "Dune/Graphics/DescriptorHeap.h"
#include "Dune/Common/Pool.h"

namespace Dune
{
	class Window;
	struct CameraComponent;
	struct BufferDesc;
	struct DirectionalLight;
	struct PointLight;
	class GraphicsElement;
	struct InstanceData;
	class Buffer;
	class Mesh;
	struct Vertex;

	// Change once per draw call
	struct InstanceData
	{
		dMatrix4x4	modelMatrix;
		dMatrix4x4	normalMatrix;
		dVec4		baseColor;
	};

	inline constexpr dU32 gs_instanceDataSize{ sizeof(InstanceData) };

	// Change once per camera
	struct CameraConstantBuffer
	{
		dMatrix4x4	viewProjMatrix;
	};

	class Renderer
	{
		friend Buffer;
	public:
		DISABLE_COPY_AND_MOVE(Renderer);

		[[nodiscard]] static Renderer& GetInstance();
		void Initialize(const Window * window);
		void Shutdown();

		[[nodiscard]] bool IsInitialized() const { return m_bIsInitialized; }

		// TODO : Generalize graphics data submission
		void							ClearGraphicsElements();
		void							RemoveGraphicsElement(EntityID id);
		void							SubmitGraphicsElement(EntityID id, Handle<Mesh> mesh, const InstanceData& instanceData);

		void							ClearPointLights();
		void							RemovePointLight(EntityID id);
		void							SubmitPointLight(EntityID id, const PointLight& light);

		void							ClearDirectionalLights();
		void							RemoveDirectionalLight(EntityID id);
		void							SubmitDirectionalLight(EntityID id, const DirectionalLight& light);

		void							CreateCamera();
		void							UpdateCamera(const CameraComponent* pCamera);

		void							Render();

		void							OnResize(int width, int height);

		[[nodiscard]] Handle<Buffer>	CreateBuffer(const BufferDesc& desc, const void* pData, dU32 size);
		void							UpdateBuffer(Handle<Buffer> handle, const void* pData, dU32 size);
		void							ReleaseBuffer(Handle<Buffer> handle);

		[[nodiscard]] Handle<Mesh>		CreateMesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		void							ReleaseMesh(Handle<Mesh> handle);
		// Temp until I can load Mesh correctly
		void							CreateDefaultMesh(); 

		void							ReleaseResource(IUnknown* resource);
		
		[[nodiscard]] ID3D12Device*		GetDevice() { Assert(m_device.Get()); return m_device.Get(); }


	private:
		Renderer() = default;
		~Renderer();
		
		void CreateFactory();
		void EnableDebugLayer() const;
		void CreateDevice();
		void CreateCommandQueues();
		void CreateSwapChain(HWND handle);
		void CreateRenderTargets();
		void CreateDepthStencil(int width, int height);
		void CreateCommandAllocators();
		void CreateSamplers();
		void CreateRootSignature();
		void CreatePipeline();
		void CreateCommandLists();
		void CreateFences();

		[[nodiscard]] Buffer&			GetBuffer(Handle<Buffer> handle);
		[[nodiscard]] Mesh&				GetMesh(Handle<Mesh> handle);
		void							ReleaseDyingResources(dU64 frameIndex);

		void InitShadowPass();
		void InitMainPass();
		void InitImGuiPass();

		void BeginFrame();
		void ExecuteShadowPass();
		void ExecuteMainPass();
		void ExecuteImGuiPass();
		void Present();
		void EndFrame();
		
		void WaitForFrame(const dU64 frameIndex);
		void WaitForCopy();

		//TEMP
		void PrepareShadowPass();
		void CreatePointLightsBuffer();
		void UpdatePointLights();
		void CreateDirectionalLightsBuffer();
		void UpdateDirectionalLights();
		void CreateInstancesDataBuffer();
		void UpdateInstancesData();

	private:
		inline static constexpr dU32						ms_frameCount{ 2 };
		inline static constexpr dU32						ms_shadowMapCount{ 1 };

		bool												m_bIsInitialized{ false };
		Handle<Mesh>										m_cubeMesh;

		Pool<Buffer>										m_bufferPool;
		Pool<Mesh>											m_meshPool;

		// Descriptor Heaps
		DescriptorHeap										m_rtvHeap;
		DescriptorHeap										m_dsvHeap;
		DescriptorHeap										m_srvHeap;
		DescriptorHeap										m_samplerHeap;

		// Frame resources
		D3D12_VIEWPORT										m_viewport{};
		D3D12_RECT											m_scissorRect{};
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_backBuffers[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_depthStencilBuffer;
		DescriptorHandle									m_backBufferViews[ms_frameCount];
		DescriptorHandle									m_depthBufferView;

		// Direct commands
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_commandAllocators[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;

		// Copy commands
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_copyCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_copyCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_copyCommandList;

		// Shadow Pass
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_shadowMaps[ms_shadowMapCount];
		Handle<Buffer>										m_shadowCameraBuffers[ms_shadowMapCount][ms_frameCount];
		DescriptorHandle									m_shadowDepthViews[ms_shadowMapCount];
		DescriptorHandle									m_shadowResourceViews[ms_shadowMapCount];
		DescriptorHandle									m_shadowMapSamplerView;

		// Main Pass
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		Handle<Buffer>										m_pointLightsBuffer[ms_frameCount]; 		
		Handle<Buffer>										m_directionalLightsBuffer[ms_frameCount]; 	
		Handle<Buffer>										m_instancesDataBuffer[ms_frameCount];
		DescriptorHandle									m_pointLightsViews[ms_frameCount];
		DescriptorHandle									m_directionalLightsViews[ms_frameCount];
		DescriptorHandle									m_instancesDataViews[ms_frameCount];

		dVector<DirectionalLight>							m_directionalLights;
		dVector<EntityID>									m_directionalLightEntities;
		dHashMap<EntityID, dU32>							m_lookupDirectionalLights;

		dVector<PointLight>									m_pointLights;
		dVector<EntityID>									m_pointLightEntities;
		dHashMap<EntityID, dU32>							m_lookupPointLights;

		dVector<InstanceData>								m_instancesData;
		dVector<EntityID>									m_graphicsEntities;
		dHashMap<EntityID, dU32>							m_lookupGraphicsElements;

		Handle<Buffer>										m_cameraMatrixBuffer;

		// ImGui Pass
		ID3D12DescriptorHeap*								m_imguiHeap{nullptr};

		// Synchronization
		dU64												m_frameIndex{0};
		dU64												m_frameNumber{0};
		Microsoft::WRL::Wrappers::Event						m_fenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_fence;
		dU64												m_fenceValues[ms_frameCount];
		Microsoft::WRL::Wrappers::Event						m_copyFenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_copyFence;
		dVector<IUnknown*>									m_dyingResources[ms_frameCount];

		// Device
		Microsoft::WRL::ComPtr<ID3D12Device>				m_device;
		Microsoft::WRL::ComPtr<IDXGIFactory4>				m_factory;
	};
}

