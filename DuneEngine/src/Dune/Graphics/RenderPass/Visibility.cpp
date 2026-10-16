#include "pch.h"
#include "Dune/Graphics/RenderPass/Visibility.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Component/Camera.h"

namespace Dune::Graphics
{
	static dMatrix4x4 ComputeShadowMatrix(const LightData& light)
	{
		dMatrix4x4 lightMatrix;
		if (light.IsPoint())
			DirectX::XMStoreFloat4x4(&lightMatrix, DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.f), 1.0f, 0.1f, light.range));
		else
		{
			dVec up{ 0.f, 1.f, 0.f, 0.f };
			dVec to{ DirectX::XMLoadFloat3(&light.direction) };
			dVec axis = DirectX::XMVector3Cross(up, to);
			if (DirectX::XMVector3Equal(axis, { 0.0f, 0.0f, 0.0f }))
				up = { 0.f, 0.f, 1.f, 0.f };

			if (light.IsSpot())
			{
				dVec eye{ light.position.x, light.position.y, light.position.z };
				dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
				dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(light.angle * 2.0f, 1.0f, 0.1f, light.range) };
				DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
			}
			else
			{
				float shadowWidth{ 36.f }; // Hardcoded for sponza
				dVec eye{ 0.f, 0.f, 0.f };
				dMatrix viewMatrix{ DirectX::XMMatrixLookToLH(eye, to, up) };
				dMatrix projectionMatrix{ DirectX::XMMatrixOrthographicLH(shadowWidth, shadowWidth, -shadowWidth, shadowWidth) };
				DirectX::XMStoreFloat4x4(&lightMatrix, viewMatrix * projectionMatrix);
			}
		}
		return lightMatrix;
	}

	static void FillLight(const Light& sceneLight, LightData& light)
	{
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
			light.angle = DirectX::XMScalarCos(sceneLight.angle);
			{
				float lightSolidAngle = 2.0f * DirectX::XM_PI * (1.0f - light.angle);
				float candelaIntensity = sceneLight.intensity / lightSolidAngle;
				light.intensity = candelaIntensity;
			}
			light.penumbra = 1.0f / (DirectX::XMScalarCos(sceneLight.angle * (1.0f - sceneLight.penumbra)) - light.angle);
			light.flags |= fIsSpot;
			break;
		}
		if (sceneLight.castShadow)
			light.flags |= fCastShadow;
	}

	VisibilityData* Visibility::Create(Renderer&)
	{
		return new VisibilityData();
	}

	void Visibility::Setup(RenderGraphBuilder&, RenderPassContext& context, VisibilityData* pData)
	{
		const FrameData& frameData = *context.pFrameData;

		pData->lights.clear();
		pData->shadowCasters.clear();
		pData->shadowMatrices.clear();
		for (const Light& sceneLight : frameData.lights)
		{
			if (sceneLight.intensity <= 0.0f)
				continue;
			LightData light{};
			FillLight(sceneLight, light);
			if (sceneLight.castShadow)
			{
				light.shadowIndex = (dU32)pData->shadowCasters.size();
				pData->shadowCasters.push_back((dU32)pData->lights.size());
				pData->shadowMatrices.push_back(ComputeShadowMatrix(light));
			}
			pData->lights.push_back(light);
		}

		// TODO: Culling
		pData->opaqueDraws.clear();
		pData->blendDraws.clear();
		for (dU32 drawIndex = 0; drawIndex < (dU32)frameData.drawItems.size(); drawIndex++)
		{
			const bool isBlend = Material::GetAlphaMode(frameData.drawItems[drawIndex].materialKey) == EAlphaMode::Blend;
			(isBlend ? pData->blendDraws : pData->opaqueDraws).push_back(drawIndex);
		}

		std::stable_sort(pData->opaqueDraws.begin(), pData->opaqueDraws.end(),
			[&](dU32 a, dU32 b) { return frameData.drawItems[a].materialKey < frameData.drawItems[b].materialKey; });

		const dVec3& eye = context.pCamera->position;
		const auto DistanceToCameraSq = [&](dU32 drawIndex)
			{
				const dMatrix4x4& objectToWorld = frameData.drawItems[drawIndex].objectToWorld;
				const float dx = objectToWorld._41 - eye.x;
				const float dy = objectToWorld._42 - eye.y;
				const float dz = objectToWorld._43 - eye.z;
				return dx * dx + dy * dy + dz * dz;
			};
		std::stable_sort(pData->blendDraws.begin(), pData->blendDraws.end(),
			[&](dU32 a, dU32 b) { return DistanceToCameraSq(a) > DistanceToCameraSq(b); });

		context.blackboard.Add<VisibleSet>(
			{
				.lights = pData->lights,
				.shadowCasters = pData->shadowCasters,
				.shadowMatrices = pData->shadowMatrices,
				.opaqueDraws = pData->opaqueDraws,
				.blendDraws = pData->blendDraws,
			});
	}

	void Visibility::Execute(RenderPassContext&, VisibilityData*)
	{
	}

	void Visibility::Destroy(Renderer&, VisibilityData* pData)
	{
		delete pData;
	}
}
