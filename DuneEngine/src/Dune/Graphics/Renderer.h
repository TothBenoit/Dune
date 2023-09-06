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
	class Shader;
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
		dMatrix		viewProjMatrix;
		dVec4		cameraWorldPos;
	};

	class Renderer
	{
		friend Buffer;
		struct InstancedBatch;
	public:
		DISABLE_COPY_AND_MOVE(Renderer);

		[[nodiscard]] static Renderer& GetInstance();
		void Initialize(const Window * window);
		void Shutdown();

		[[nodiscard]] bool IsInitialized() const { return m_bIsInitialized; }

		// TODO : Generalize graphics data submission
		// Hint : Double buffered graphics objects. One is used by the Engine the other by the Renderer.

		void							ClearGraphicsElements();
		void							RemoveGraphicsElement(EntityID id, Handle<Mesh> mesh);
		void							SubmitGraphicsElement(EntityID id, Handle<Mesh> mesh, const InstanceData& instanceData);

		void							ClearPointLights();
		void							RemovePointLight(EntityID id);
		void							SubmitPointLight(EntityID id, const dVec3& color, float intensity, const dVec3& pos, float radius);

		void							ClearDirectionalLights();
		void							RemoveDirectionalLight(EntityID id);
		void							SubmitDirectionalLight(EntityID id, const dVec3& color, const dVec3& direction, float intensity, const dMatrix4x4& viewProj);

		void							CreateCamera();
		void							UpdateCamera(const CameraComponent* pCamera, const dVec3& pos);

		void							Render();

		void							OnResize(int width, int height);

		[[nodiscard]] Handle<Buffer>	CreateBuffer(const BufferDesc& desc);
		void							UpdateBuffer(Handle<Buffer> handle, const void* pData, dU32 size);
		void							ReleaseBuffer(Handle<Buffer> handle);

		[[nodiscard]] Handle<Mesh>		CreateMesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		void							ReleaseMesh(Handle<Mesh> handle);
		// Temp until I can load Mesh correctly
		void							CreateDefaultMesh(); 

		void							ReleaseResource(IUnknown* resource);
		
		[[nodiscard]] ID3D12Device*		GetDevice() { Assert(m_device.Get()); return m_device.Get(); }
		[[nodiscard]] Handle<Mesh>		GetDefaultMesh() const { Assert(m_device.Get()); return m_defaultMesh; }
		[[nodiscard]] dU32				GetShadowMapCount() const { Assert(m_device.Get()); return ms_shadowMapCount; }

	private:

		inline static constexpr dU32 ms_frameCount{ 2 };
		inline static constexpr dU32 ms_shadowMapCount{ 4 };

		struct InstancedBatch
		{
			dVector<InstanceData>		instancesData;
			dVector<EntityID>			graphicsEntities;
			dHashMap<EntityID, dU32>	lookupGraphicsElements;

			Handle<Buffer>				instancesDataBuffer[ms_frameCount];
			DescriptorHandle			instancesDataViews[ms_frameCount];
		};

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

		// TODO : Find a nice form for this case.
		// Update is called each frame. Not good.
		void CreatePointLightsBuffer();
		void UpdatePointLights();
		void CreateDirectionalLightsBuffer();
		void UpdateDirectionalLights();
		void UpdateInstancesData();

		// Temp : Until I use Material component
		void CreateDefaultShader();
	private:
		bool												m_bIsInitialized{ false };
		Handle<Mesh>										m_defaultMesh;
		Handle<Shader>										m_defaultShader;

		Pool<Buffer>										m_bufferPool;
		Pool<Mesh>											m_meshPool;
		Pool<Shader>										m_shaderPool;

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
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_shadowMaps;
		Handle<Buffer>										m_shadowCameraBuffers[ms_shadowMapCount][ms_frameCount];
		DescriptorHandle									m_shadowMapsDepthViews[ms_shadowMapCount];
		DescriptorHandle									m_shadowMapsResourceView;
		DescriptorHandle									m_shadowMapsSamplerView;

		// Main Pass
		Handle<Buffer>										m_pointLightsBuffer[ms_frameCount]; 		
		Handle<Buffer>										m_directionalLightsBuffer[ms_frameCount]; 	
		DescriptorHandle									m_pointLightsViews[ms_frameCount];
		DescriptorHandle									m_directionalLightsViews[ms_frameCount];

		dVector<DirectionalLight>							m_directionalLights;
		dVector<EntityID>									m_directionalLightEntities;
		dHashMap<EntityID, dU32>							m_lookupDirectionalLights;

		dVector<PointLight>									m_pointLights;
		dVector<EntityID>									m_pointLightEntities;
		dHashMap<EntityID, dU32>							m_lookupPointLights;

		dHashMap<ID::IDType, InstancedBatch>				m_batches;

		Handle<Buffer>										m_cameraMatrixBuffer;

		// ImGui Pass
		DescriptorHandle									m_imguiDescriptorHandle;

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

