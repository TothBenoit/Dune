#include <Dune.h>
#include <thread>
#include <chrono>
#include <Dune/Core/Graphics/Shaders/PBR.h>
#include <Dune/Core/Graphics/RHI/Texture.h>
#include <Dune/Core/Graphics/RHI/Device.h>
#include <Dune/Core/Renderer.h>
#include <Dune/Utilities/DDSLoader.h>

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

void Test(Graphics::Device* pDevice, Scene* pScene)
{
	Renderer renderer;
	renderer.Initialize(pDevice);
	float dt = 0.f;
	while (renderer.UpdateSceneView(dt))
	{	
		auto start = std::chrono::high_resolution_clock::now();

		renderer.RenderScene(*pScene);
		auto end = std::chrono::high_resolution_clock::now();
		dt = (float)std::chrono::duration<float>(end - start).count();;
	}

	renderer.Destroy();
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	Graphics::Device device{};
	device.Initialize();

	Graphics::Texture* pAlbedoTexture = Graphics::DDSTexture::CreateTextureFromFile(&device, "res\\testAlbedoMips.DDS");
	Graphics::Texture* pNormalTexture = Graphics::DDSTexture::CreateTextureFromFile(&device, "res\\testNormalMips.DDS");
	Graphics::Mesh cube{};
	cube.Initialize(&device, cubeIndices, _countof(cubeIndices), cubeVertices, _countof(cubeVertices), sizeof(Vertex));

	Scene scene{};
	entt::entity cubeEntity = scene.CreateEntity("Cube");

	Transform& transform = scene.registry.emplace<Transform>(cubeEntity);
	transform.position.z = 2;
	RenderData& renderData = scene.registry.emplace<RenderData>(cubeEntity);
	
	//renderData.pAlbedo = pAlbedoTexture;
	//renderData.pNormal = pNormalTexture;
	renderData.pMesh = &cube;

	dVector<std::thread> tests;
	dU32 windowCount{ 1 };
	tests.reserve(windowCount);
	for (dU32 i{ 0 }; i < windowCount; i++)
	{
		tests.emplace_back(std::thread(&Test, &device, &scene));
	}

	for (dU32 i{ 0 }; i < windowCount; i++)
	{
		tests[i].join();
	}

	scene.registry.destroy(cubeEntity);

	delete pAlbedoTexture;
	delete pNormalTexture;

	cube.Destroy();
	device.Destroy();

	return 0;
}