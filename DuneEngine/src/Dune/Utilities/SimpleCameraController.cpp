#include "pch.h"
#include "Dune/Utilities/SimpleCameraController.h"
#include "Dune/Core/Input.h"

namespace Dune
{
	SimpleCameraController::SimpleCameraController(const Graphics::Camera& camera)
		: m_camera{ camera }
	{
		dMatrix4x4 rotationMatrix;
		DirectX::XMStoreFloat4x4(&rotationMatrix,DirectX::XMMatrixLookToLH({ 0.0f,0.0f,0.0f }, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_camera.target), DirectX::XMLoadFloat3(&m_camera.position))), DirectX::XMLoadFloat3(&m_camera.up)));		
		float sy = sqrtf(powf(rotationMatrix._11, 2.0f) + powf(rotationMatrix._21, 2.0f));
		m_rotation.x = atan2(-rotationMatrix._31, sy);
		m_rotation.y = atan2(-rotationMatrix._23, rotationMatrix._22);
	}

	void SimpleCameraController::Update(float deltaTime, const Input* pInput)
	{

		if (!pInput->GetKey(KeyCode::AltKey) && pInput->GetMouseButton(2))
			UpdateFirstPersonControls(deltaTime, pInput);
		else
			UpdateEditorControls(deltaTime, pInput);
	}

	void SimpleCameraController::UpdateFirstPersonControls(float deltaTime, const Input* pInput)
	{
		dVec3 translate{ 0.f,0.f,0.f };
		dVec2 rotation{ 0.0f, 0.0f };

		//Get input
		if (pInput->GetKey(KeyCode::Q))
		{
			translate.x = -1.f;
		}
		if (pInput->GetKey(KeyCode::D))
		{
			translate.x = 1.f;
		}

		if (pInput->GetKey(KeyCode::A))
		{
			translate.y = -1.f;
		}
		if (pInput->GetKey(KeyCode::E))
		{
			translate.y = 1.f;
		}

		if (pInput->GetKey(KeyCode::Z))
		{
			translate.z = 1.f;
		}
		if (pInput->GetKey(KeyCode::S))
		{
			translate.z = -1.f;
		}
		
		const float clampedDeltaTime = DirectX::XMMin(deltaTime, 0.033f);

		//Add rotation
		constexpr float turnSpeed = DirectX::XMConvertToRadians(45.f);
		constexpr float xRotationClampValues[]{ DirectX::XMConvertToRadians(-89.99f), DirectX::XMConvertToRadians(89.99f) };
		m_rotation.x = std::fmodf(m_rotation.x + pInput->GetMouseDeltaX() * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		m_rotation.y = std::clamp(std::fmodf(m_rotation.y + pInput->GetMouseDeltaY() * turnSpeed * clampedDeltaTime, DirectX::XM_2PI), xRotationClampValues[0], xRotationClampValues[1]);

		//Compute quaternion from camera rotation
		DirectX::XMVECTOR xQuat{ DirectX::XMQuaternionRotationAxis(DirectX::XMLoadFloat3(&m_camera.up), m_rotation.x) };
		DirectX::XMVECTOR yQuat{ DirectX::XMQuaternionRotationAxis({ 1.0f, 0.0f, 0.0f }, m_rotation.y) };
		DirectX::XMVECTOR quat = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(yQuat, xQuat));

		//Apply camera rotation to translation
		DirectX::XMStoreFloat3(&translate,
			DirectX::XMVector3Rotate(
				DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&translate)),
				quat
			)
		);

		//Apply translation
		const float speed = (pInput->GetKey(KeyCode::ShiftKey)) ? 25.f : 5.f;
		m_camera.position.x += translate.x * speed * clampedDeltaTime;
		m_camera.position.y += translate.y * speed * clampedDeltaTime;
		m_camera.position.z += translate.z * speed * clampedDeltaTime;

		//Compute camera view matrix
		DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&m_camera.position);
		DirectX::XMStoreFloat3(&m_camera.target, DirectX::XMVectorAdd(eye, DirectX::XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, quat)));
	}

	void SimpleCameraController::UpdateEditorControls(float deltaTime, const Input* pInput)
	{
		// Alt + Left Click : Rotate around target
		// Alt + Middle Click : Pan
		// Alt + Right Click : Zoom
	}
}

