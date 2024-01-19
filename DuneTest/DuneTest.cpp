#include <Dune.h>
#include <thread>
#include <mutex>

std::mutex g_mutex; // API is not thread-safe yet

using namespace Dune;

struct Vertex
{
	dVec3 vPos;
	dVec3 vNormal;
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

Handle<Graphics::Pipeline> CreateTrianglePipeline(Graphics::Device* pDevice)
{
	const wchar_t* args[] = { L"-Zi" };

	Handle<Graphics::Shader> triVertexShader =
		Graphics::CreateShader
		({
			.stage = Graphics::EShaderStage::Vertex,
			.filePath = L"Shaders\\Test.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
			});

	Handle<Graphics::Shader> triPixelShader =
		Graphics::CreateShader
		({
			.stage = Graphics::EShaderStage::Pixel,
			.filePath = L"Shaders\\Test.hlsl",
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
			});

	Handle<Graphics::Pipeline> triPipeline =
		Graphics::CreateGraphicsPipeline
		({
			.vertexShader = triVertexShader,
			.pixelShader = triPixelShader,
			.bindingLayout =
			{
				.slots =
				{
					{.type = Graphics::EBindingType::Constant, .byteSize = 16, .visibility = Graphics::EShaderVisibility::Vertex },
					{.type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::Pixel },
				},
				.slotCount = 2
			},
			.inputLayout = { Graphics::VertexInput{ "POSITION", 0, Graphics::EFormat::R32G32B32_FLOAT, 0, 0, false } },
			.renderTargetsFormat = { Graphics::EFormat::R8G8B8A8_UNORM },
			.pDevice = pDevice
			});

	Graphics::ReleaseShader(triPixelShader);
	Graphics::ReleaseShader(triVertexShader);

	return triPipeline;
}

void Test(Graphics::Device* pDevice)
{
	g_mutex.lock();
	Graphics::View* pView{ Graphics::CreateView({.pDevice = pDevice}) };

	Handle<Graphics::Pipeline> triPipeline = CreateTrianglePipeline(pDevice);

	dU16 triIndices[]{ 0, 1, 2 };
	dVec3 triVertices[]{ { -0.5f, -0.5f, 0.0 }, { 0.0f, 0.5f, 0.0 }, { 0.5f, -0.5f, 0.0 } };
	Handle<Graphics::Mesh> triangle = Graphics::CreateMesh(pView, triIndices, 3, triVertices, 3, sizeof(dVec3));
	Handle<Graphics::Mesh> cube = Graphics::CreateMesh(pView, cubeIndices, 3, cubeVertices, 3, sizeof(Vertex));

	Graphics::Command* pCommand =
		Graphics::CreateCommand
		({
			.type = Graphics::ECommandType::Graphics,
			.pView = pView
		});

	const Graphics::Mesh& mesh = Graphics::GetMesh(triangle);

	float offset[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
	float color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
	float direction{ 1.0 };

	Handle<Graphics::Buffer> colorBuff = Graphics::CreateBuffer({ .debugName = L"ColorBuffer", .byteSize = 16, .usage = Graphics::EBufferUsage::Constant, .memory = Graphics::EBufferMemory::CPU, .pData = color, .pView = pView });

	g_mutex.unlock();
	dU32 frameCount = 0;
	while (Graphics::ProcessViewEvents(pView))
	{
		// Game goes here
		if (std::abs(offset[0]) > 1.0f)
		{
			offset[0] = 1.0f * direction;
			color[0] = 1.0f * direction;
			color[1] = -1.0f * direction;
			direction *= -1.f;
		}
		offset[0] += 0.005f * direction;
		color[0] += 0.005f * direction;
		color[1] += -0.005f * direction;

		Graphics::BeginFrame(pView);
		Graphics::MapBuffer(colorBuff, color, 16);
		Graphics::ResetCommand(pCommand);
		Graphics::SetPipeline(pCommand, triPipeline);
		Graphics::SetRenderTarget(pCommand, pView);
		Graphics::ClearRenderTarget(pCommand, pView);
		Graphics::PushGraphicsConstants(pCommand, 0, offset, 16);
		Graphics::PushGraphicsBuffer(pCommand, 1, colorBuff);
		Graphics::BindIndexBuffer(pCommand, mesh.GetIndexBufferHandle());
		Graphics::BindVertexBuffer(pCommand, mesh.GetVertexBufferHandle());
		Graphics::DrawIndexedInstanced(pCommand, 3, 1);
		Graphics::SubmitCommand(pView, pCommand);
		Graphics::EndFrame(pView);
		frameCount++;
	}

	g_mutex.lock();
	Graphics::ReleaseBuffer(colorBuff);
	Graphics::ReleaseMesh(cube);
	Graphics::ReleaseMesh(triangle);
	Graphics::ReleasePipeline(triPipeline);
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