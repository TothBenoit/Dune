#include "pch.h"
#include "Dune/Graphics/RenderPass/Visibility.h"
#include "Dune/Graphics/RenderPass/Outputs.h"
#include "Dune/Graphics/FrameData.h"

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

	void Visibility::Compute(RenderPassContext& context, VisibilityData& data)
	{
		const FrameData& frameData = *context.pFrameData;

		data.lights.clear();
		data.shadowCasters.clear();
		data.shadowMatrices.clear();
		for (LightData light : frameData.lights)
		{
			if (light.HasShadow())
			{
				light.shadowIndex = (dU32)data.shadowCasters.size();
				data.shadowCasters.push_back((dU32)data.lights.size());
				data.shadowMatrices.push_back(ComputeShadowMatrix(light));
			}
			data.lights.push_back(light);
		}

		// TODO: Culling
		data.opaqueDraws.clear();
		data.blendDraws.clear();
		for (dU32 drawIndex = 0; drawIndex < (dU32)frameData.drawItems.size(); drawIndex++)
		{
			const bool isBlend = Material::GetAlphaMode(frameData.drawItems[drawIndex].materialKey) == EAlphaMode::Blend;
			(isBlend ? data.blendDraws : data.opaqueDraws).push_back(drawIndex);
		}

		std::stable_sort(data.opaqueDraws.begin(), data.opaqueDraws.end(),
			[&](dU32 a, dU32 b) { return frameData.drawItems[a].materialKey < frameData.drawItems[b].materialKey; });

		const dVec3& eye = context.pView->position;
		const auto DistanceToCameraSq = [&](dU32 drawIndex)
			{
				const dMatrix4x4& objectToWorld = frameData.drawItems[drawIndex].objectToWorld;
				const float dx = objectToWorld._41 - eye.x;
				const float dy = objectToWorld._42 - eye.y;
				const float dz = objectToWorld._43 - eye.z;
				return dx * dx + dy * dy + dz * dz;
			};
		std::stable_sort(data.blendDraws.begin(), data.blendDraws.end(),
			[&](dU32 a, dU32 b) { return DistanceToCameraSq(a) > DistanceToCameraSq(b); });

		context.blackboard.Add<VisibleSet>(
			{
				.lights = data.lights,
				.shadowCasters = data.shadowCasters,
				.shadowMatrices = data.shadowMatrices,
				.opaqueDraws = data.opaqueDraws,
				.blendDraws = data.blendDraws,
			});
	}
}
