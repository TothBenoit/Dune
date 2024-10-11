#pragma once
#include "Dune/Common/Handle.h"
#include "Dune/Core/Graphics/Format.h"
#include "Dune/Core/Graphics/RHI/Buffer.h"
#include "Dune/Core/Graphics/RHI/Texture.h"
#include "Dune/Core/Graphics/Window.h"

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

	// Device
	Device*							CreateDevice();
	void							DestroyDevice(Device* pDevice);

	// View

	struct ViewDesc
	{
		Device* pDevice{ nullptr };
		WindowDesc windowDesc;
		dU8 backBufferCount{ 2 };
		void (*pOnResize)(View*, void*) { nullptr };
		void* pOnResizeData{ nullptr };
	};

	class View
	{
	public:
		[[nodiscard]] const Window* GetWindow() const { return m_pWindow; } 
		[[nodiscard]] Device* GetDevice() { return m_pDeviceInterface; }
	protected:
		Window* m_pWindow{ nullptr };
		Device* m_pDeviceInterface{ nullptr };
	};

	[[nodiscard]]  View*			CreateView(const ViewDesc& desc);
	void							DestroyView(View* pView);
	bool							ProcessViewEvents(View* pView);
	void							BeginFrame(View* pView);	
	void							EndFrame(View* pView);
	[[nodiscard]] const Input*		GetInput(View* pView);

	// Mesh

	[[nodiscard]] Handle<Mesh>		CreateMesh(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
	[[nodiscard]] Handle<Mesh>		CreateMesh(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
	void							ReleaseMesh(Device* pDevice, Handle<Mesh> handle);
	[[nodiscard]] const Mesh&		GetMesh(Device* pDevice, Handle<Mesh> handle);

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

	[[nodiscard]] Handle<Shader>	CreateShader(Device* pDevice, const ShaderDesc& desc);
	void							ReleaseShader(Device* pDevice, Handle<Shader> handle);

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
	};

	[[nodiscard]] Handle<Pipeline>			CreateGraphicsPipeline(	Device* pDevice, const GraphicsPipelineDesc& desc);
	void									ReleasePipeline( Device* pDevice, Handle<Pipeline> handle);

	// BindGroup

	// Compute Pipeline
	
	struct DirectCommandDesc
	{
		View* pView;
	};

	[[nodiscard]] DirectCommand*	CreateDirectCommand(const DirectCommandDesc& desc);
	void							DestroyCommand(DirectCommand* pCommand);
	void							SubmitCommand(Device* pDevice, DirectCommand* pCommand);
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

	enum class ECommandBuffering : dU32
	{
		None = 1,
		Double = 2,
		Triple = 3
	};

	struct ComputeCommandDesc
	{
		Device* pDevice;
		ECommandBuffering buffering;
	};

	[[nodiscard]] ComputeCommand*	CreateComputeCommand(const ComputeCommandDesc& desc);
	void							DestroyCommand(ComputeCommand* pCommand);
	void							SubmitCommand(Device* pDevice, ComputeCommand* pCommand);

	struct CopyCommandDesc
	{
		Device* pDevice;
		ECommandBuffering buffering;
	};

	[[nodiscard]] CopyCommand*		CreateCopyCommand(const CopyCommandDesc& desc);
	void							DestroyCommand(CopyCommand* pCommand);
	void							SubmitCommand(Device* pDevice, ComputeCommand* pCommand);
	
}