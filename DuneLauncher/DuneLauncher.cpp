#include <Dune.h>
#include <cmath>

using namespace Dune;

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Graphics::Initialize();
	Graphics::Device* pDevice{ Graphics::CreateDevice() };
	Graphics::View* pView{ Graphics::CreateView({.pDevice = pDevice})};
	const wchar_t* args[] = { L"-Zi" };

	Handle<Graphics::Shader> vertexShader =	
		Graphics::CreateShader 
			({
				.stage = Graphics::EShaderStage::Vertex,
				.filePath = L"C:\\Users\\Benoit\\source\\repos\\Dune\\DuneEngine\\src\\Dune\\Core\\Graphics\\Shaders\\Test.hlsl",
				.entryFunc = L"VSMain",
				.args = args,
				.argsCount = _countof(args),
			});

	Handle<Graphics::Shader> pixelShader =
		Graphics::CreateShader
			({
				.stage = Graphics::EShaderStage::Pixel,
				.filePath = L"C:\\Users\\Benoit\\source\\repos\\Dune\\DuneEngine\\src\\Dune\\Core\\Graphics\\Shaders\\Test.hlsl",
				.entryFunc = L"PSMain",
				.args = args,
				.argsCount = _countof(args),
			});
	
	Handle<Graphics::Pipeline> pipeline =
		Graphics::CreateGraphicsPipeline
			({
				.vertexShader = vertexShader,
				.pixelShader = pixelShader,
				.bindingLayout = 
				{ 
					.slots = 
					{ 
						{ .type = Graphics::EBindingType::Constant, .byteSize = 16, .visibility = Graphics::EShaderVisibility::Vertex }, 
						{ .type = Graphics::EBindingType::Buffer, .visibility = Graphics::EShaderVisibility::Pixel }, 
					}, 
					.slotCount = 2 
				},
				.inputLayout = { Graphics::VertexInput{ "POSITION", 0, Graphics::EFormat::R32G32B32_FLOAT, 0, 0, false } },
				.renderTargetsFormat = { Graphics::EFormat::R8G8B8A8_UNORM },
				.pDevice = pDevice
			});
	
	dU16 indices[]{ 0, 1, 2 };
	dVec3 vertices[]{ { -0.5f, -0.5f, 0.0 }, { 0.0f, 0.5f, 0.0 }, { 0.5f, -0.5f, 0.0 } };
	Handle<Graphics::Mesh> triangle = Graphics::CreateMesh(pView, indices, 3, vertices, 3, sizeof(dVec3));
	
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
		Graphics::SetPipeline(pCommand, pipeline);
		Graphics::SetRenderTarget(pCommand, pView);
		//Graphics::ClearRenderTarget(pCommand, pView);
		Graphics::PushGraphicsConstants(pCommand, 0, offset, 16);
		Graphics::PushGraphicsBuffer(pCommand, 1, colorBuff);
		Graphics::BindIndexBuffer(pCommand, mesh.GetIndexBufferHandle());
		Graphics::BindVertexBuffer(pCommand, mesh.GetVertexBufferHandle());
		Graphics::DrawIndexedInstanced(pCommand, 3, 1);
		Graphics::SubmitCommand(pView, pCommand);
		Graphics::EndFrame(pView);
	}

	Graphics::ReleaseBuffer(colorBuff);
	Graphics::ReleaseMesh(triangle);
	Graphics::ReleaseShader(pixelShader);
	Graphics::ReleaseShader(vertexShader);
	Graphics::ReleasePipeline(pipeline);
	Graphics::DestroyCommand(pCommand);
	Graphics::DestroyView(pView);
	Graphics::DestroyDevice(pDevice);
	Graphics::Shutdown();

	return 0;
}