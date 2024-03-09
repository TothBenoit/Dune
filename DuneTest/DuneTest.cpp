#include <Dune.h>
#include <thread>
#include <mutex>
#include <Dune/Core/Graphics/Shaders/PBR.h>
#include <Dune/Utilities/DDSLoader.h>

std::mutex g_mutex; // API is not thread-safe yet

using namespace Dune;

struct Vertex
{
	dVec3 vPos;
	dVec3 vNormal;
	dVec3 vTangent;
	dVec2 vUV;
};

static const dU16 cubeIndices[]
{
	0, 1, 2, 0, 2, 3,			//Face
	4, 6, 5, 4, 7, 6,			//Back
	10, 11, 9, 10, 9, 8,		//Left
	13, 12, 14, 13, 14, 15,		//Right
	16, 18, 19, 16, 19, 17,		//Top
	22, 20, 21, 22, 21, 23		//Bottom
};

static const Vertex cubeVertices[] =
{
	{ {-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } }, // 0
	{ {-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } }, // 1
	{ { 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } }, // 2
	{ { 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } }, // 3

	{ {-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f }, {-1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } }, // 4
	{ {-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f }, {-1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } }, // 5
	{ { 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f }, {-1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } }, // 6
	{ { 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f }, {-1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } }, // 7

	{ {-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f } }, // 8
	{ {-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f } }, // 9
	{ {-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f } }, // 10
	{ {-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f } }, // 11

	{ { 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f } }, // 12
	{ { 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } }, // 13
	{ { 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } }, // 14
	{ { 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f } }, // 15

	{ {-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } }, // 16
	{ { 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f } }, // 17
	{ {-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f } }, // 18
	{ { 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f } }, // 19

	{ {-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } }, // 20
	{ { 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } }, // 21
	{ {-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } }, // 22
	{ { 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } }, // 23
};

Handle<Graphics::Pipeline> CreatePBRPipeline(Graphics::Device* pDevice)
{
	const wchar_t* args[] = { L"-Zi", L"-I Shaders\\" };

	Handle<Graphics::Shader> pbrVertexShader =
		Graphics::CreateShader
		({
			.stage = Graphics::EShaderStage::Vertex,
			.filePath = L"Shaders\\PBR.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
		});

	Handle<Graphics::Shader> pbrPixelShader =
		Graphics::CreateShader
		({
			.stage = Graphics::EShaderStage::Pixel,
			.filePath = L"Shaders\\PBR.hlsl",
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
		});

	Handle<Graphics::Pipeline> pbrPipeline =
		Graphics::CreateGraphicsPipeline
		({
			.vertexShader = pbrVertexShader,
			.pixelShader = pbrPixelShader,
			.bindingLayout =
			{
				.slots =
				{
					{ .type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::All },
					{ .type = Graphics::EBindingType::Group, .groupDesc = { .resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel},
					{ .type = Graphics::EBindingType::Group, .groupDesc = { .resourceCount = 1 }, .visibility = Graphics::EShaderVisibility::Pixel},
					{ .type = Graphics::EBindingType::Constant, .byteSize = 16, .visibility = Graphics::EShaderVisibility::Pixel },
					{ .type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::Vertex },
				},
				.slotCount = 5
			},
			.inputLayout =
				{
					Graphics::VertexInput { .pName = "POSITION", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 0, .bPerInstance = false },
					Graphics::VertexInput { .pName = "NORMAL", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 12, .bPerInstance = false },
					Graphics::VertexInput { .pName = "TANGENT", .index = 0, .format = Graphics::EFormat::R32G32B32_FLOAT, .slot = 0, .byteAlignedOffset = 24, .bPerInstance = false },
					Graphics::VertexInput { .pName = "UV", .index = 0, .format = Graphics::EFormat::R32G32_FLOAT, .slot = 0, .byteAlignedOffset = 36, .bPerInstance = false }
				},
			.depthStencilState = { .bDepthEnabled = true, .bDepthWrite = true },
			.renderTargetsFormat = { Graphics::EFormat::R8G8B8A8_UNORM },
			.depthStencilFormat = Graphics::EFormat::D16_UNORM,
			.pDevice = pDevice
		});

	Graphics::ReleaseShader(pbrPixelShader);
	Graphics::ReleaseShader(pbrVertexShader);

	return pbrPipeline;
}

struct OnResizeData
{
	Handle<Graphics::Buffer>* globalsBuffer;
	Handle<Graphics::Texture>* depthBuffer;
};

void OnResize(Graphics::View* pView, void* pData)
{
	OnResizeData& data = *(OnResizeData*)pData;

	dU32 width = pView->GetWidth();
	dU32 height = pView->GetHeight();

	if (data.globalsBuffer->IsValid())
	{
		dMatrix view{ DirectX::XMMatrixLookAtLH({0.0f, 1.0f, -1.0f}, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }) };
		dMatrix proj{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(85.f), (float)width / (float)height, 0.01f, 1000.f) };

		Graphics::PBRGlobals globals;
		DirectX::XMStoreFloat4x4(&globals.viewProjectionMatrix, view * proj);
		DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));
		Graphics::UploadBuffer(*data.globalsBuffer, &globals, sizeof(Graphics::PBRGlobals));
	}

	if (data.depthBuffer->IsValid())
	{
		Graphics::ReleaseTexture(*data.depthBuffer);
		*data.depthBuffer = Graphics::CreateTexture({ .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { width, height, 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = {1.f, 1.f, 1.f, 1.f}, .pView = pView });
	}
}

void Test(Graphics::Device* pDevice)
{
	g_mutex.lock();

	Handle<Graphics::Buffer> globalsBuffer;
	Handle<Graphics::Texture> depthBuffer;
	OnResizeData onResizeData{ .globalsBuffer = &globalsBuffer, .depthBuffer = &depthBuffer };
	Graphics::View* pView{ Graphics::CreateView({.pDevice = pDevice, .pOnResize = OnResize, .pOnResizeData = &onResizeData}) };

	Handle<Graphics::Pipeline> pbrPipeline = CreatePBRPipeline(pDevice);
	Handle<Graphics::Mesh> cube = Graphics::CreateMesh(pView, cubeIndices, _countof(cubeIndices), cubeVertices, _countof(cubeVertices), sizeof(Vertex));

	Graphics::Command* pCommand =
		Graphics::CreateCommand
		({
			.type = Graphics::ECommandType::Graphics,
			.pView = pView
		});

	const Graphics::Mesh& mesh = Graphics::GetMesh(cube);

	Graphics::DDSTexture ddsTexture;
	if (ddsTexture.Load("res\\testAlbedo.DDS") != Graphics::DDSResult::ESucceed)
		Assert(0);
	void* pData = ddsTexture.GetData();
	const Graphics::DDSHeader* pHeader = ddsTexture.GetHeader();
	Handle<Graphics::Texture> texture = Graphics::CreateTexture({ .debugName = L"TestTexture", .usage = Graphics::ETextureUsage::SRV, .dimensions = { pHeader->height, pHeader->width, pHeader->depth + 1 }, .format = Graphics::EFormat::BC7_UNORM, .clearValue = {0.f, 0.f, 0.f, 0.f}, .pView = pView, .pData = pData, .byteSize = pHeader->height * pHeader->width * ( pHeader->depth + 1 ) * ( pHeader->pixelFormat.size / 8 ) });
	ddsTexture.Destroy();

	if (ddsTexture.Load("res\\testNormal.DDS") != Graphics::DDSResult::ESucceed)
		Assert(0);
	pData = ddsTexture.GetData();
	pHeader = ddsTexture.GetHeader();
	Handle<Graphics::Texture> normalTexture = Graphics::CreateTexture({ .debugName = L"TestNormalTexture", .usage = Graphics::ETextureUsage::SRV, .dimensions = { pHeader->height, pHeader->width, pHeader->depth + 1 }, .format = Graphics::EFormat::BC7_UNORM, .clearValue = {0.f, 0.f, 0.f, 0.f}, .pView = pView, .pData = pData, .byteSize = pHeader->height * pHeader->width * (pHeader->depth + 1) * (pHeader->pixelFormat.size / 8) });
	ddsTexture.Destroy();

	dMatrix view { DirectX::XMMatrixLookAtLH({0.0f, 1.0f, -1.0f}, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }) };
	dMatrix proj{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(85.f), (float)pView->GetWidth() / (float)pView->GetHeight(), 0.01f, 1000.f)};

	Graphics::PBRGlobals globals;
	DirectX::XMStoreFloat4x4(&globals.viewProjectionMatrix, view * proj);
	DirectX::XMStoreFloat3(&globals.sunDirection, DirectX::XMVector3Normalize({ 0.1f, -1.0f, 0.9f }));
	globalsBuffer = Graphics::CreateBuffer({ .debugName = L"GlobalsBuffer", .byteSize = sizeof(Graphics::PBRGlobals), .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::GPU, .pData = &globals, .pView = pView});
	
	float angle = 0.f;
	dMatrix rotation{ };

	dMatrix initialModel{ DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f) };
	Handle<Graphics::Buffer> instanceBuffer = Graphics::CreateBuffer({ .debugName = L"InstanceBuffer", .byteSize = sizeof(Graphics::PBRInstance), .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::CPU, .pData = &initialModel, .pView = pView });
	depthBuffer = Graphics::CreateTexture({ .debugName = L"DepthBuffer", .usage = Graphics::ETextureUsage::DSV, .dimensions = { pView->GetWidth(), pView->GetHeight(), 1}, .format = Graphics::EFormat::D16_UNORM, .clearValue = {1.f, 1.f, 1.f, 1.f}, .pView = pView });

	dVec4 material{ 0.0f, 1.0f, 0.5f, 1.0f };

	g_mutex.unlock();
	dU32 frameCount = 0;
	while (Graphics::ProcessViewEvents(pView))
	{
		angle = fmodf(angle + 0.1f, 360.f);
		Graphics::BeginFrame(pView);
		dMatrix model{ DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(angle)) * initialModel };
		Graphics::MapBuffer(instanceBuffer, &model, sizeof(Graphics::PBRInstance));
		Graphics::ResetCommand(pCommand);
		Graphics::SetPipeline(pCommand, pbrPipeline);
		Graphics::SetRenderTarget(pCommand, pView, depthBuffer);
		Graphics::ClearRenderTarget(pCommand, pView);
		Graphics::ClearDepthBuffer(pCommand, depthBuffer);
		Graphics::PushGraphicsBuffer(pCommand, 0, globalsBuffer);
		Graphics::BindGraphicsTexture(pCommand, 1, texture);
		Graphics::BindGraphicsTexture(pCommand, 2, normalTexture);
		Graphics::PushGraphicsConstants(pCommand, 3, &material, 16);
		Graphics::PushGraphicsBuffer(pCommand, 4, instanceBuffer);
		Graphics::BindIndexBuffer(pCommand, mesh.GetIndexBufferHandle());
		Graphics::BindVertexBuffer(pCommand, mesh.GetVertexBufferHandle());
		Graphics::DrawIndexedInstanced(pCommand, mesh.GetIndexCount(), 1);
		Graphics::SubmitCommand(pView, pCommand);
		Graphics::EndFrame(pView);
		frameCount++;
	}

	g_mutex.lock();
	Graphics::ReleaseMesh(cube);
	Graphics::ReleaseBuffer(globalsBuffer);
	Graphics::ReleaseBuffer(instanceBuffer);
	Graphics::ReleaseTexture(depthBuffer);
	Graphics::ReleaseTexture(texture);
	Graphics::ReleaseTexture(normalTexture);
	Graphics::ReleasePipeline(pbrPipeline);
	Graphics::DestroyCommand(pCommand);
	Graphics::DestroyView(pView);
	g_mutex.unlock();
}


int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Graphics::Initialize();
	Graphics::Device* pDevice = Graphics::CreateDevice();
	dVector<std::thread> tests;
	dU32 testCount{ 5 };
	tests.reserve(testCount);

	for (dU32 i{ 0 }; i < testCount; i++)
	{
		tests.emplace_back(std::thread(&Test, pDevice));
	}

	for (dU32 i{ 0 }; i < testCount; i++)
	{
		tests[i].join();
	}

	Graphics::DestroyDevice(pDevice);
	Graphics::Shutdown();

	return 0;
}