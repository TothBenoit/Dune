#include <Dune.h>
#include <algorithm>
#include "SimpleCameraController.h"
#include "Dune/Platform/Input.h"

namespace Dune
{
	static dQuat ComputeRotation(const dVec2& rotation)
	{
		const DirectX::XMVECTOR xQuat{ DirectX::XMQuaternionRotationAxis({ 0.0f, 1.0f, 0.0f }, rotation.x) };
		const DirectX::XMVECTOR yQuat{ DirectX::XMQuaternionRotationAxis({ 1.0f, 0.0f, 0.0f }, rotation.y) };
		return DirectX::XMQuaternionMultiply(yQuat, xQuat);
	}

	Transform SimpleCameraController::GetTransform() const
	{
		return { .position = m_position, .rotation = ComputeRotation(m_rotation), .scale = 1.0f };
	}

	void SimpleCameraController::Update(float deltaTime, const Input& input)
	{

		if (!input.GetKey(KeyCode::AltKey) && input.GetMouseButton(2))
			UpdateFirstPersonControls(deltaTime, input);
		else
			UpdateEditorControls(deltaTime, input);
	}

	void SimpleCameraController::UpdateFirstPersonControls(float deltaTime, const Input& input)
	{
		dVec3 translate{ 0.f,0.f,0.f };

		//Get input
		if (input.GetKey(KeyCode::Q))
		{
			translate.x = -1.f;
		}
		if (input.GetKey(KeyCode::D))
		{
			translate.x = 1.f;
		}

		if (input.GetKey(KeyCode::A))
		{
			translate.y = -1.f;
		}
		if (input.GetKey(KeyCode::E))
		{
			translate.y = 1.f;
		}

		if (input.GetKey(KeyCode::Z))
		{
			translate.z = 1.f;
		}
		if (input.GetKey(KeyCode::S))
		{
			translate.z = -1.f;
		}
		
		const float clampedDeltaTime = DirectX::XMMin(deltaTime, 0.033f);

		//Add rotation
		constexpr float radiansPerPixel = DirectX::XMConvertToRadians(0.75f);
		dVec2 rotation{ input.GetMouseDeltaX() * radiansPerPixel, input.GetMouseDeltaY() * radiansPerPixel };
		constexpr float yRotationClampValues[]{ DirectX::XMConvertToRadians(-89.99f), DirectX::XMConvertToRadians(89.99f) };
		m_rotation.x = std::fmodf(m_rotation.x + rotation.x, DirectX::XM_2PI);
		m_rotation.y = std::clamp(std::fmodf(m_rotation.y + rotation.y, DirectX::XM_2PI), yRotationClampValues[0], yRotationClampValues[1]);

		const DirectX::XMVECTOR quat{ ComputeRotation(m_rotation) };

		const float speed = (input.GetKey(KeyCode::ShiftKey)) ? 7.5f : 2.5f;
		const float velocity = speed * clampedDeltaTime;

		if (translate.x != 0.0f || translate.z != 0.0f)
		{
			dVec3 move{};
			DirectX::XMStoreFloat3(&move,
				DirectX::XMVector3Rotate(
					DirectX::XMVector3Normalize(DirectX::XMVectorSet(translate.x, 0.0f, translate.z, 0.0f)),
					quat
				)
			);
			m_position.x += move.x * velocity;
			m_position.y += move.y * velocity;
			m_position.z += move.z * velocity;
		}

		m_position.y += translate.y * velocity;
	}

	void SimpleCameraController::UpdateEditorControls(float deltaTime, const Input& input)
	{
		// Alt + Left Click : Rotate around target
		// Alt + Middle Click : Pan
		// Alt + Right Click : Zoom
	}
}
