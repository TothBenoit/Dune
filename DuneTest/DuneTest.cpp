#include <Dune.h>
#include <DirectXMath.h>

using namespace DirectX;

class DuneLauncher : public Dune::Application
{
public:

	void OnStart() override
	{
		Dune::EntityID entity = Dune::EngineCore::CreateEntity("My cube");
		Dune::EngineCore::AddComponent<Dune::GraphicsComponent>(entity).mesh = Dune::EngineCore::GetDefaultMesh();
		
		Dune::TransformComponent& cubeTransform = Dune::EngineCore::ModifyComponentUnsafe<Dune::TransformComponent>(entity);
		cubeTransform.position.z = 3.f;
		cubeTransform.position.y = -1.25f;

		Dune::EntityID sunEntity = Dune::EngineCore::CreateEntity("Sun");
		Dune::DirectionalLightComponent& light = Dune::EngineCore::AddComponent<Dune::DirectionalLightComponent>(sunEntity);
		light.intensity = 0.8f;
		Dune::TransformComponent& lightTransform = Dune::EngineCore::ModifyComponentUnsafe<Dune::TransformComponent>(sunEntity);
		lightTransform.rotation.y = 180;

		followingLightEntity = Dune::EngineCore::CreateEntity("FollowingLight");
		Dune::PointLightComponent& followingLight = Dune::EngineCore::AddComponent<Dune::PointLightComponent>(followingLightEntity);
		followingLight.intensity = 0.38f;
		followingLight.radius = 100;

		Dune::Camera& camera = Dune::EngineCore::ModifyCamera();
		camera.rotation.x = 20.f * 3.14f / 180.f;
	}

	void OnUpdate(float dt) override
	{
		if (Dune::EngineCore::IsAlive(followingLightEntity)) // Can be removed by the ImGui interface
		{
			const Dune::Camera& camera{ Dune::EngineCore::GetCamera() };
			Dune::TransformComponent& lightTransform = Dune::EngineCore::ModifyComponentUnsafe<Dune::TransformComponent>(followingLightEntity);
			lightTransform.position = camera.position;
		}
	}

	Dune::EntityID followingLightEntity;
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