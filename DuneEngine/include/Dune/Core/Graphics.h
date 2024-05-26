#pragma once
#include "Dune/Common/Handle.h"

namespace Dune
{
	class Input;
}

namespace Dune::Graphics
{
	struct Device;
	class View;
	struct DirectCommand;
	struct ComputeCommand;
	struct CopyCommand;
	class Buffer;
	class Texture;
	class Mesh;
	struct Vertex;
	class Shader;
	class Pipeline;
	class Window;

	void Initialize();
	void Shutdown();

	// Device
	Device*							CreateDevice();
	void							DestroyDevice(Device* pDevice);

	// View
	struct WindowDesc
	{
		void* parent{ nullptr };
		dU32 width{ 1600 };
		dU32 height{ 900 };
		dWString title{ L"Dune Window" };
	};

	struct ViewDesc
	{
		Graphics::Device* pDevice{ nullptr };
		WindowDesc windowDesc;
		dU8 backBufferCount{ 2 };
		void (*pOnResize)(View*, void*) { nullptr };
		void* pOnResizeData{ nullptr };
	};

	class View
	{
	public:
		[[nodiscard]] const Window* GetWindow() const { return m_pWindow; }
	protected:
		Window* m_pWindow{ nullptr };
	};

	[[nodiscard]]  View*			CreateView(const ViewDesc& desc);
	void							DestroyView(View* pView);
	bool							ProcessViewEvents(View* pView);
	void							BeginFrame(View* pView);
	void							SubmitCommand(View* pView, DirectCommand* pCommand);
	void							EndFrame(View* pView);
	[[nodiscard]] const Input*		GetInput(View* pView);

	// Buffer
	enum class EBufferUsage
	{
		Vertex,
		Index,
		Constant,
		Structured,
	};

	enum class EBufferMemory
	{
		CPU,
		GPU,
		GPUStatic
	};

	struct BufferDesc
	{
		const wchar_t*	debugName = nullptr;

		dU64			byteSize{ 0 };
		EBufferUsage	usage{ EBufferUsage::Constant };
		EBufferMemory	memory{ EBufferMemory::CPU };
		const void*		pData{ nullptr };
		dU32			byteStride{ 0 }; // for structured, vertex and index buffer
		View*			pView{ nullptr };
	};

	[[nodiscard]] Handle<Buffer>	CreateBuffer(const BufferDesc& desc);
	void							ReleaseBuffer(Handle<Buffer> handle);
	void							UploadBuffer(Handle<Buffer> handle, const void* pData, dU64 byteOffset, dU64 byteSize);
	void							MapBuffer(Handle<Buffer> handle, const void* pData, dU64 byteOffset, dU64 byteSize);
	// Texture

	enum class ETextureUsage
	{
		RTV,
		DSV,
		SRV,
		UAV,
	};

	enum class EFormat
	{
		UNKNOWN = 0,

		R32G32B32A32_FLOAT			= 2,
		R32G32B32A32_UINT			= 3,
		R32G32B32A32_SINT			= 4,

		R32G32B32_FLOAT				= 6,
		R32G32B32_UINT				= 7,
		R32G32B32_SINT				= 8,

		R16G16B16A16_FLOAT			= 10,
		R16G16B16A16_UNORM			= 11,
		R16G16B16A16_UINT			= 12,
		R16G16B16A16_SNORM			= 13,
		R16G16B16A16_SINT			= 14,

		R32G32_FLOAT				= 16,
		R32G32_UINT					= 17,
		R32G32_SINT					= 18,

		D32_FLOAT_S8X24_UINT		= 20,

		R10G10B10A2_UNORM			= 24,
		R10G10B10A2_UINT			= 25,

		R11G11B10_FLOAT				= 26,

		R8G8B8A8_UNORM				= 28,
		R8G8B8A8_UNORM_SRGB			= 29,
		R8G8B8A8_UINT				= 30,
		R8G8B8A8_SNORM				= 31,
		R8G8B8A8_SINT				= 32,

		R16G16_FLOAT				= 34,
		R16G16_UNORM				= 35,
		R16G16_UINT					= 36,
		R16G16_SNORM				= 37,
		R16G16_SINT					= 38,

		D32_FLOAT					= 40,
		R32_FLOAT					= 41,
		R32_UINT					= 42,
		R32_SINT					= 43,

		D24_UNORM_S8_UINT			= 45,

		R8G8_UNORM					= 49,
		R8G8_UINT					= 50,
		R8G8_SNORM					= 51,
		R8G8_SINT					= 52,

		R16_FLOAT					= 54,
		D16_UNORM					= 55,
		R16_UNORM					= 56,
		R16_UINT					= 57,
		R16_SNORM					= 58,
		R16_SINT					= 59,

		R8_UNORM					= 61,
		R8_UINT						= 62,
		R8_SNORM					= 63,
		R8_SINT						= 64,

		BC1_UNORM					= 71,
		BC1_UNORM_SRGB				= 72,

		BC2_UNORM					= 74,
		BC2_UNORM_SRGB				= 75,

		BC3_UNORM					= 77,
		BC3_UNORM_SRGB				= 78,

		BC4_UNORM					= 80,
		BC4_SNORM					= 81,

		BC5_UNORM					= 83,
		BC5_SNORM					= 84,

		BC6H_UF16					= 95,
		BC6H_SF16					= 96,

		BC7_UNORM					= 98,
		BC7_UNORM_SRGB				= 99,
	};

	struct TextureDesc
	{
		const wchar_t* debugName{ nullptr };

		ETextureUsage usage{ ETextureUsage::RTV };
		dU32 dimensions[3]{ 1, 1, 1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		float clearValue[4]{0.f, 0.f, 0.f, 0.f};
		View* pView{ nullptr };
		void* pData{ nullptr };
		dU64 byteSize{ 0 };
	};

	[[nodiscard]] Handle<Texture>	CreateTexture(const TextureDesc& desc);
	void							ReleaseTexture(Handle<Texture> handle);

	// Mesh

	[[nodiscard]] Handle<Mesh>		CreateMesh(View* pView, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
	[[nodiscard]] Handle<Mesh>		CreateMesh(View* pView, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
	void							ReleaseMesh(Handle<Mesh> handle);
	[[nodiscard]] const Mesh&		GetMesh(Handle<Mesh> handle);

	// Shader

	enum class EShaderStage
	{
		Vertex,
		Pixel,
		Compute,
	};

	enum class EShaderVisibility
	{
		Vertex,
		Pixel,
		All,
		Count
	};

	enum class EBindingType
	{
		Constant,
		Buffer,
		Resource,
		UAV,
		Group,
		Samplers,
	};

	struct BindingGroup
	{
		dU32 bufferCount{ 0 };
		dU32 uavCount{ 0 };
		dU32 resourceCount{ 0 };
	};

	struct BindingSlot
	{
		EBindingType type;
		union
		{
			BindingGroup	groupDesc;
			dU32			byteSize;
			dU32			samplerCount;
		};
		EShaderVisibility visibility{ EShaderVisibility::All };
	};

	struct BindingLayout
	{
		BindingSlot slots[64];
		dU8 slotCount{ 0 };
	};

	struct ShaderDesc
	{
		EShaderStage	stage;
		const wchar_t*	filePath{ nullptr };
		const wchar_t*	entryFunc{ nullptr };
		// TODO : use span
		const wchar_t** args;
		dU32			argsCount;
	};

	[[nodiscard]] Handle<Shader>	CreateShader(const ShaderDesc& desc);
	void							ReleaseShader(Handle<Shader> handle);

	// Graphics Pipeline

	enum class ECullingMode
	{
		NONE = 1,
		FRONT = 2,
		BACK = 3
	};

	struct RasterizerState
	{
		ECullingMode cullingMode { ECullingMode::BACK };
		dS32 depthBias { 0 };
		float depthBiasClamp { 0.0f };
		float slopeScaledDepthBias { 0.0f };
		bool bDepthClipEnable : 1 { true };
		bool bWireframe : 1 { false };
	};

	enum class ECompFunc
	{
		NONE,
		NEVER = 1,
		LESS = 2,
		EQUAL = 3,
		LESS_EQUAL = 4,
		GREATER = 5,
		NOT_EQUAL = 6,
		GREATER_EQUAL = 7,
		ALWAYS = 8
	};

	struct DepthStencilState
	{
		bool		bDepthEnabled	: 1	{ false };
		bool		bDepthWrite		: 1	{ false };
		ECompFunc	bDepthFunc			{ ECompFunc::LESS_EQUAL };
		// TODO: Add stencil
	};

	struct VertexInput
	{
		const char* pName		{ nullptr };
		dU32 index				{ 0 };
		EFormat format			{ EFormat::R32G32B32A32_FLOAT };
		dU32 slot				{ 0 };
		dU32 byteAlignedOffset	{ 0 };
		bool bPerInstance		{ false };
	};

	struct GraphicsPipelineDesc
	{
		Handle<Shader>			vertexShader;
		Handle<Shader>			pixelShader;
		BindingLayout			bindingLayout;
		// TODO : Use span
		dVector<VertexInput>	inputLayout;
		RasterizerState			rasterizerState;
		DepthStencilState		depthStencilState;
		// TODO : Use span or array[8] like dx12
		dVector<EFormat>		renderTargetsFormat;
		EFormat					depthStencilFormat;
		Device*					pDevice{ nullptr };
	};

	[[nodiscard]] Handle<Pipeline>			CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
	void									ReleasePipeline(Handle<Pipeline> handle);

	// BindGroup

	// Compute Pipeline

	enum class ECommandBuffering : dU32
	{
		None = 1,
		Double = 2,
		Triple = 3
	};
	
	struct DirectCommandDesc
	{
		View* pView;
	};

	[[nodiscard]] DirectCommand*	CreateDirectCommand(const DirectCommandDesc& desc);
	void							DestroyCommand(DirectCommand* pCommand);
	void							DestroyCommand(ComputeCommand* pCommand);
	void							DestroyCommand(CopyCommand* pCommand);
	void							ResetCommand(DirectCommand* pCommand);
	void							ResetCommand(DirectCommand* pCommand, Handle<Pipeline>);
	void							SetPipeline(DirectCommand* pCommand, Handle<Pipeline> handle);
	void							SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget);
	void							SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget, Handle<Texture> depthBuffer);
	void							SetRenderTarget(DirectCommand* pCommand, View* pView);
	void							SetRenderTarget(DirectCommand* pCommand, View* pView, Handle<Texture> depthBuffer);
	void							ClearRenderTarget(DirectCommand* pCommand, Handle<Texture> handle);
	void							ClearRenderTarget(DirectCommand* pCommand, View* pView);
	void							ClearDepthBuffer(DirectCommand* pCommand, Handle<Texture> depthBuffer);
	void							PushGraphicsConstants(DirectCommand* pCommand, dU32 slot, void* pData, dU32 byteSize);
	void							PushGraphicsBuffer(DirectCommand* pCommand, dU32 slot, Handle<Buffer> handle);
	void							PushGraphicsResource(DirectCommand* pCommand, dU32 slot, Handle<Buffer> handle);
	//void							PushGraphicsBindGroup(DirectCommand* pCommand, dU32 slot, Handle<BindGroup> handle);
	void							BindGraphicsTexture(DirectCommand* pCommand, dU32 slot, Handle<Texture> handle);
	void							BindIndexBuffer(DirectCommand* pCommand, Handle<Buffer> handle);
	void							BindVertexBuffer(DirectCommand* pCommand, Handle<Buffer> handle);
	void							DrawIndexedInstanced(DirectCommand* pCommand, dU32 indexCount, dU32 instanceCount);

	struct ComputeCommandDesc
	{
		Device* pDevice;
		ECommandBuffering buffering;
	};

	[[nodiscard]] ComputeCommand* CreateComputeCommand(const ComputeCommandDesc& desc);

	struct CopyCommandDesc
	{
		Device* pDevice;
		ECommandBuffering buffering;
	};

	[[nodiscard]] CopyCommand* CreateCopyCommand(const CopyCommandDesc& desc);
	
}