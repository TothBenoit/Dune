#pragma once

#include "Dune/Graphics/DescriptorHeap.h"
#include "Dune/Common/Pool.h"

namespace Dune
{
	class Window;
	struct CameraComponent;
	struct DirectionalLight;
	struct PointLight;
	class GraphicsElement;
	struct InstanceData;
	class Buffer;
	struct BufferDesc;
	class Texture;
	struct TextureDesc;
	class Shader;
	struct ShaderDesc;
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
		dMatrix		viewProjMatrix;
		dVec4		cameraWorldPos;
	};

	class Renderer
	{
		friend Buffer;
		friend Texture;
		friend Shader;
		struct InstancedBatch;
	public:
		DISABLE_COPY_AND_MOVE(Renderer);

		[[nodiscard]] static Renderer& GetInstance();
		void Initialize(const Window * window);
		void Shutdown();

		[[nodiscard]] bool IsInitialized() const { return m_bIsInitialized; }

		// TODO : Redo API to let the user handle graphics elements on its side

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
		void							UploadBuffer(Handle<Buffer> handle, const void* pData, dU32 size);
		void							MapBuffer(Handle<Buffer> handle, const void* pData, dU32 size);
		void							ReleaseBuffer(Handle<Buffer> handle);

		[[nodiscard]] Handle<Texture>	CreateTexture(const TextureDesc& desc);
		void							ReleaseTexture(Handle<Texture> handle);

		[[nodiscard]] Handle<Mesh>		CreateMesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices);
		void							ReleaseMesh(Handle<Mesh> handle);
		// Temp until I can load Mesh correctly
		void							CreateDefaultMesh(); 

		[[nodiscard]] Handle<Shader>	CreateShader(const ShaderDesc& desc);
		void							ReleaseShader(Handle<Shader> handle);

		void							ReleaseResource(IUnknown* resource);
		
		[[nodiscard]] ID3D12Device*		GetDevice() { Assert(m_device.Get()); return m_device.Get(); }
		[[nodiscard]] Handle<Mesh>		GetDefaultMesh() const { Assert(m_device.Get()); return m_defaultMesh; }
		[[nodiscard]] dU64				GetElaspedFrame() { return m_elapsedFrame; }
		[[nodiscard]] dU64				GetFrameIndex() { return m_frameIndex; }
		[[nodiscard]] static dU32		GetShadowMapCount() { return ms_shadowMapCount; }
		[[nodiscard]] static dU32		GetFrameCount() { return ms_frameCount; }

	private:

		inline static constexpr dU32 ms_frameCount{ 2 };
		inline static constexpr dU32 ms_shadowMapCount{ 4 };

		struct InstancedBatch
		{
			dVector<InstanceData>		instancesData;
			dVector<EntityID>			graphicsEntities;
			dHashMap<EntityID, dU32>	lookupGraphicsElements;

			Handle<Buffer>				instancesDataBuffer;
		};

		Renderer() = default;
		~Renderer();
		
		void CreateFactory();
		void EnableDebugLayer() const;
		void CreateDevice();
		void CreateCommandQueues();
		void CreateSwapChain(HWND handle);
		void CreateRenderTargets();
		void CreateDepthStencil(dU32 width, dU32 height);
		void CreateCommandAllocators();
		void CreateSamplers();
		void CreateCommandLists();
		void CreateFences();

		[[nodiscard]] Buffer&			GetBuffer(Handle<Buffer> handle);
		[[nodiscard]] Texture&			GetTexture(Handle<Texture> handle);
		[[nodiscard]] Mesh&				GetMesh(Handle<Mesh> handle);
		[[nodiscard]] Shader&			GetShader(Handle<Shader> handle);
		void							ReleaseDyingResources(dU64 frameIndex);

		void InitShadowPass();
		void InitMainPass();
		void InitPostProcessPass();
		void InitImGuiPass();

		void BeginFrame();
		void ExecuteShadowPass();
		void ExecuteMainPass();
		void ExecuteImGuiPass();
		void ExecutePostProcessPass();
		void Present();
		void EndFrame();
		
		void WaitForFrame(const dU64 frameIndex);
		void WaitForCopy();

		void Resize();

		// TODO : Find a nice form for this case.
		// Update is called each frame. Not good.
		void UpdatePointLights();
		void UpdateDirectionalLights();
		void UpdateInstancesData();
		void UpdateCameraBuffer();

		// Temp : Until I use Material component
		void CreateDefaultShader();
		void CreatePostProcessShader();
	private:
		bool												m_bIsInitialized{ false };
		bool												m_needResize{ false };
		bool												m_needCameraUpdate{ false };

		Handle<Mesh>										m_defaultMesh;
		Handle<Shader>										m_defaultShader;
		Handle<Shader>										m_postProcessShader;

		Pool<Buffer>										m_bufferPool;
		Pool<Texture>										m_texturePool;
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
		float												m_FOV{ 85.f };
		float												m_farPlane{ 1000.f };
		float												m_nearPlane{ 1.0f };
		dMatrix												m_viewMatrix{};
		dVec3												m_cameraPosition{ 0.0f, 0.0f, 0.0f };
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_backBuffers[ms_frameCount];
		DescriptorHandle									m_backBufferViews[ms_frameCount];
		Handle<Texture>										m_depthStencilBuffer;

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
		Handle<Buffer>										m_shadowCameraBuffers[ms_shadowMapCount];
		DescriptorHandle									m_shadowMapsDepthViews[ms_shadowMapCount];
		DescriptorHandle									m_shadowMapsResourceView;
		DescriptorHandle									m_shadowMapsSamplerView;

		// Main Pass
		Handle<Texture>										m_intermediateRenderTarget;
		Handle<Buffer>										m_pointLightsBuffer; 		
		Handle<Buffer>										m_directionalLightsBuffer; 	

		dVector<DirectionalLight>							m_directionalLights;
		dVector<EntityID>									m_directionalLightEntities;
		dHashMap<EntityID, dU32>							m_lookupDirectionalLights;

		dVector<PointLight>									m_pointLights;
		dVector<EntityID>									m_pointLightEntities;
		dHashMap<EntityID, dU32>							m_lookupPointLights;

		dHashMap<ID::IDType, InstancedBatch>				m_batches;

		Handle<Buffer>										m_cameraMatrixBuffer;
		
		// PostProcess pass
		Handle<Buffer>										m_fullScreenIndices;
		Handle<Buffer>										m_postProcessGlobals;

		// ImGui Pass
		DescriptorHandle									m_imguiDescriptorHandle;

		// Synchronization
		dU64												m_frameIndex{ 0 };
		dU64												m_elapsedFrame{ 0 };
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

