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

	void Test()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			Dune::EntityID entity = Dune::EngineCore::CreateEntity("My cube");
			Dune::EngineCore::AddComponent<Dune::GraphicsComponent>(entity);
			if (Dune::TransformComponent* cubeTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(entity))
			{
				cubeTransform->position.z = 3.f;
				cubeTransform->position.y = -1.25f;
				cubeTransform->hasChanged = true;
			}

			Dune::EntityID cameraID = Dune::EngineCore::GetCameraID();
			Dune::EngineCore::AddComponent<Dune::PointLightComponent>(cameraID);
			if (Dune::PointLightComponent* light = Dune::EngineCore::GetComponent<Dune::PointLightComponent>(cameraID))
			{
				light->radius = 10.f;
			}
			if (Dune::TransformComponent* camTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(cameraID))
			{
				camTransform->rotation.x = 20.f * 3.14f/180.f;
				camTransform->hasChanged = true;
			}

			flyingLight1 = Dune::EngineCore::CreateEntity("Flying light1");
			Dune::EngineCore::AddComponent<Dune::PointLightComponent>(flyingLight1);
			if (Dune::PointLightComponent* light = Dune::EngineCore::GetComponent<Dune::PointLightComponent>(flyingLight1))
			{
				light->intensity = 5.f;
				light->radius = 150.f;
				light->color = { 0.f,1.f,0.f };
			}

			flyingLight2 = Dune::EngineCore::CreateEntity("Flying light2");
			Dune::EngineCore::AddComponent<Dune::PointLightComponent>(flyingLight2);
			if (Dune::PointLightComponent* light = Dune::EngineCore::GetComponent<Dune::PointLightComponent>(flyingLight2))
			{
				light->intensity = 5.f;
				light->radius = 120.f;
				light->color = { 1.f,0.f,0.f };
			}

			flyingLight3 = Dune::EngineCore::CreateEntity("Flying light3");
			Dune::EngineCore::AddComponent<Dune::PointLightComponent>(flyingLight3);
			if (Dune::PointLightComponent* light = Dune::EngineCore::GetComponent<Dune::PointLightComponent>(flyingLight3))
			{
				light->intensity = 5.f;
				light->radius = 90.f;
				light->color = { 0.f,0.f,1.f };
			}

			didTestOnce = true;
		}

	}

	void OnUpdate(float dt) override
	{
		static float time = 0.f;
		time += dt;
		Test();

		if (Dune::TransformComponent* flyingLightTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(flyingLight1))
		{
			flyingLightTransform->position.x = 100 * cosf(time / 1.75f);
			flyingLightTransform->position.y = 100 * sinf(time / 1.5f);
			flyingLightTransform->position.z = 100 * cosf(time / 1.25f);
			flyingLightTransform->hasChanged = true;
		}

		if (Dune::TransformComponent* flyingLightTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(flyingLight2))
		{
			flyingLightTransform->position.x = 100 * sinf(time/1.f);
			flyingLightTransform->position.y = 100 * cosf(time/1.5f);
			flyingLightTransform->position.z = 100 * cosf(time/2.f);
			flyingLightTransform->hasChanged = true;
		}

		if (Dune::TransformComponent* flyingLightTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(flyingLight3))
		{
			flyingLightTransform->position.x = 100 * cos(time * 1.25f);
			flyingLightTransform->position.y = 100 * sin(time * 2.f);
			flyingLightTransform->position.z = 100 * sin(time * 1.5f);
			flyingLightTransform->hasChanged = true;
		}

	}

	Dune::EntityID flyingLight1;
	Dune::EntityID flyingLight2;
	Dune::EntityID flyingLight3;

};

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}