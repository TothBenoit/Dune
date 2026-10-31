#include "pch.h"
#include "Dune/World/RenderBridge.h"
#include "Dune/Game/GameScene.h"
#include "Dune/Game/Component/Light.h"
#include "Dune/Game/Component/Camera.h"
#include "Dune/Game/Component/Transform.h"
#include "Dune/Game/Component/MeshInstance.h"
#include "Dune/Graphics/RenderScene.h"

namespace Dune
{
	using namespace Graphics;

	static LightData ExtractLight(const Light& sceneLight)
	{
		LightData light{};
		light.color = sceneLight.color;
		switch (sceneLight.type)
		{
		case ELightType::Directional:
			light.intensity = sceneLight.intensity;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			break;
		case ELightType::Point:
		{
			float lightSolidAngle = 4.0f * DirectX::XM_PI;
			float candelaIntensity = sceneLight.intensity / lightSolidAngle;
			light.intensity = candelaIntensity;
		}
		light.range = sceneLight.range;
		light.position = sceneLight.position;
		light.flags |= fIsPoint;
		break;
		case ELightType::Spot:
			light.range = sceneLight.range;
			light.position = sceneLight.position;
			DirectX::XMStoreFloat3(&light.direction, DirectX::XMVector3Normalize(DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(sceneLight.direction.x), DirectX::XMConvertToRadians(sceneLight.direction.y), DirectX::XMConvertToRadians(sceneLight.direction.z)))));
			const float halfAngle = std::clamp(sceneLight.halfAngle, DirectX::XMConvertToRadians(1.0f), DirectX::XMConvertToRadians(89.0f));
			light.cosAngle = DirectX::XMScalarCos(halfAngle);
			{
				float lightSolidAngle = 2.0f * DirectX::XM_PI * (1.0f - light.cosAngle);
				float candelaIntensity = sceneLight.intensity / lightSolidAngle;
				light.intensity = candelaIntensity;
			}
			light.penumbra = 1.0f / (DirectX::XMScalarCos(halfAngle * (1.0f - sceneLight.penumbra)) - light.cosAngle);
			light.flags |= fIsSpot;
			break;
		}
		if (sceneLight.castShadow)
			light.flags |= fCastShadow;
		return light;
	}

	static RenderObject ExtractObject(const Transform& transform, const MeshInstance& meshInstance)
	{
		RenderObject object{ .mesh = meshInstance.mesh, .materials = meshInstance.materials };
		DirectX::XMStoreFloat4x4(&object.objectToWorld,
			DirectX::XMMatrixScalingFromVector({ transform.scale, transform.scale, transform.scale }) *
			DirectX::XMMatrixRotationQuaternion(transform.rotation) *
			DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.position))
		);
		return object;
	}

	ViewData RenderBridge::MakeView(const Camera& camera)
	{
		ViewData view;
		ComputeViewProjectionMatrix(camera, &view.view, &view.projection, &view.viewProjection);
		view.position = camera.position;
		return view;
	}

	void RenderBridge::Update(const FrameContext& frameContext, const GameScene& gameScene, RenderScene& renderScene)
	{
		renderScene.BeginChangelog(frameContext.frameNumber);
		for (const EntityID entity : gameScene.GetChanges<Transform>())
			SyncObject(gameScene, renderScene, entity);
		for (const EntityID entity : gameScene.GetChanges<MeshInstance>())
			SyncObject(gameScene, renderScene, entity);
		for (const EntityID entity : gameScene.GetChanges<Light>())
			SyncLight(gameScene, renderScene, entity);
		for (const EntityID entity : gameScene.GetDestroyedEntities())
		{
			SyncObject(gameScene, renderScene, entity);
			SyncLight(gameScene, renderScene, entity);
		}
	}

	void RenderBridge::Reset()
	{
		m_objects.clear();
		m_lights.clear();
	}

	void RenderBridge::SyncObject(const GameScene& gameScene, RenderScene& renderScene, EntityID entity)
	{
		const Transform* pTransform = gameScene.TryGet<Transform>(entity);
		const MeshInstance* pMeshInstance = gameScene.TryGet<MeshInstance>(entity);
		const auto it = m_objects.find(entity);
		if (pTransform && pMeshInstance)
		{
			const RenderObject object = ExtractObject(*pTransform, *pMeshInstance);
			if (it != m_objects.end())
				renderScene.SetObject(it->second, object);
			else
				m_objects.emplace(entity, renderScene.CreateObject(object));
		}
		else if (it != m_objects.end())
		{
			renderScene.DestroyObject(it->second);
			m_objects.erase(it);
		}
	}

	void RenderBridge::SyncLight(const GameScene& gameScene, RenderScene& renderScene, EntityID entity)
	{
		const Light* pLight = gameScene.TryGet<Light>(entity);
		const auto it = m_lights.find(entity);
		if (pLight && pLight->intensity > 0.0f)
		{
			const LightData light = ExtractLight(*pLight);
			if (it != m_lights.end())
				renderScene.SetLight(it->second, light);
			else
				m_lights.emplace(entity, renderScene.CreateLight(light));
		}
		else if (it != m_lights.end())
		{
			renderScene.DestroyLight(it->second);
			m_lights.erase(it);
		}
	}

#ifdef _DEBUG
	void RenderBridge::Validate(const GameScene& gameScene, const RenderScene& renderScene) const
	{
		dSizeT objectCount{ 0 };
		gameScene.View<Transform, MeshInstance>().each([&](EntityID entity, const Transform& transform, const MeshInstance& meshInstance)
			{
				const auto it = m_objects.find(entity);
				Assert(it != m_objects.end());
				const RenderObject expected = ExtractObject(transform, meshInstance);
				Assert(memcmp(&expected, &renderScene.GetObject(it->second), sizeof(RenderObject)) == 0);
				objectCount++;
			});
		Assert(objectCount == m_objects.size());

		dSizeT lightCount{ 0 };
		gameScene.View<Light>().each([&](EntityID entity, const Light& sceneLight)
			{
				if (sceneLight.intensity <= 0.0f)
					return;
				const auto it = m_lights.find(entity);
				Assert(it != m_lights.end());
				const LightData expected = ExtractLight(sceneLight);
				Assert(memcmp(&expected, &renderScene.GetLight(it->second), sizeof(LightData)) == 0);
				lightCount++;
			});
		Assert(lightCount == m_lights.size());
	}
#endif
}
