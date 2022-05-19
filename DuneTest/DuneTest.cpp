#include <Dune.h>
#include <DirectXMath.h>

using namespace DirectX;

class DuneLauncher : public Dune::Application
{
public:
	DuneLauncher()
	{

	}

	~DuneLauncher()
	{

	}

	void Test1()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			srand((unsigned int)time(NULL));

			for (int i = 0; i < 1000; i++)
			{
				Dune::EntityID entity = Dune::EngineCore::CreateEntity("Node");
				if ((rand() % 11) < 3)
				{
					Dune::EngineCore::RemoveEntity(entity);
				}
			}
			didTestOnce = true;
		}
	}

	void Test2()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			Dune::GraphicsRenderer& graphicsRenderer = Dune::GraphicsCore::GetGraphicsRenderer();

			const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
			XMMATRIX ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(90.f));

			// Update the view matrix.
			const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
			const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
			const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
			XMMATRIX ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

			// Update the projection matrix.
			float aspectRatio = 1600.f / static_cast<float>(900.f);
			XMMATRIX ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.f), aspectRatio, 0.1f, 1000.0f);

			// Update the MVP matrix
			XMMATRIX mvpMatrix = ModelMatrix * ViewMatrix * ProjectionMatrix;
			Dune::dVector<Dune::dU32> indices =
			{
				0, 1, 2, 0, 2, 3,
				4, 6, 5, 4, 7, 6,
				4, 5, 1, 4, 1, 0,
				3, 2, 6, 3, 6, 7,
				1, 5, 6, 1, 6, 2,
				4, 0, 3, 4, 3, 7
			};

			// Define the geometry for a triangle.
			Dune::dVector<Dune::Vertex> vertices =
			{
				{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f} }, // 0
				{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f} }, // 1
				{ {0.5f,  0.5f, -0.5f},		{1.0f, 1.0f, 0.0f, 1.0f} }, // 2
				{ {0.5f, -0.5f, -0.5f},		{1.0f, 0.0f, 0.0f, 1.0f} }, // 3
				{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f} }, // 4
				{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f} }, // 5
				{ {0.5f,  0.5f,  0.5f},		{1.0f, 1.0f, 1.0f, 1.0f} }, // 6
				{ {0.5f, -0.5f,  0.5f},		{1.0f, 0.0f, 1.0f, 1.0f} },  // 7
			};

			Dune::dMatrix4x4* mvp = new Dune::dMatrix4x4;
			XMStoreFloat4x4(mvp, mvpMatrix);

			Dune::Mesh* mesh = new Dune::Mesh(indices, vertices);
			mesh->UploadBuffers();
			Dune::dString path = "bla";
			Dune::Shader* shader = new Dune::Shader(path);
			Dune::GraphicsElement* elem = new Dune::GraphicsElement(*mesh, *shader, *mvp);

			graphicsRenderer.AddGraphicsElement(*elem);

			didTestOnce = true;
		}
	}

	void OnUpdate() override
	{
		//Test1();
		Test2();
	}

};

int main(int argc, char** argv)
{
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}