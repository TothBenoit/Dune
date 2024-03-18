#include "pch.h"
#include "Dune/Utilities/SimpleCameraController.h"
#include "Dune/Core/Input.h"

namespace Dune
{
	SimpleCameraController::SimpleCameraController(const Graphics::Camera& camera)
		: m_camera{ camera }
	{
		dVec direction = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_camera.target), DirectX::XMLoadFloat3(&m_camera.position)));
		// TODO : Fix math
		dVec3 axis;
		DirectX::XMStoreFloat3(&axis, DirectX::XMVector3AngleBetweenVectors(direction, { 0.0f, 0.0f, 1.0f }));
		DirectX::XMStoreFloat3(&m_rotation, DirectX::XMQuaternionRotationRollPitchYaw(axis.x, axis.y, axis.z));
	}

	void SimpleCameraController::Update(float deltaTime, const Input* pInput)
	{
		dVec3 translate{ 0.f,0.f,0.f };
		dVec3 rotation{ 0.f,0.f,0.f };
		bool cameraHasMoved = false;
			
		//Get input
		if (pInput->GetKey(KeyCode::Q))
		{
			translate.x = -1.f;
			cameraHasMoved = true;
		}
		if (pInput->GetKey(KeyCode::D))
		{
			translate.x = 1.f;
			cameraHasMoved = true;
		}
			
		if (pInput->GetKey(KeyCode::A))
		{
			translate.y = -1.f;
			cameraHasMoved = true;
		}
		if (pInput->GetKey(KeyCode::E))
		{
			translate.y = 1.f;
			cameraHasMoved = true;
		}
			
		if (pInput->GetKey(KeyCode::Z))
		{
			translate.z = 1.f;
			cameraHasMoved = true;
		}
		if (pInput->GetKey(KeyCode::S))
		{
			translate.z = -1.f;
			cameraHasMoved = true;
		}
		if (pInput->GetMouseButton(2))
		{
			rotation.x = pInput->GetMouseDeltaY();
			rotation.y = pInput->GetMouseDeltaX();
			cameraHasMoved = true;
		}
			
		const float clampedDeltaTime = DirectX::XMMin(deltaTime, 0.033f);
			
		//Add rotation
		constexpr float turnSpeed = DirectX::XMConvertToRadians(45.f);
		constexpr float xRotationClampValues[] { DirectX::XMConvertToRadians(-89.99f), DirectX::XMConvertToRadians(89.99f) };
		m_rotation.x = std::clamp(std::fmodf( m_rotation.x + rotation.x * turnSpeed * clampedDeltaTime, DirectX::XM_2PI), xRotationClampValues[0], xRotationClampValues[1]);
		m_rotation.y = std::fmodf( m_rotation.y + rotation.y * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
		m_rotation.z = std::fmodf( m_rotation.z + rotation.z * turnSpeed * clampedDeltaTime, DirectX::XM_2PI);
			
		//Compute quaternion from camera rotation
		DirectX::XMVECTOR quat{ DirectX::XMQuaternionRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z) };
		quat = DirectX::XMQuaternionNormalize(quat);
		
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
		DirectX::XMStoreFloat3(&m_camera.target, DirectX::XMVectorAdd(eye, DirectX::XMVector3Rotate({ 0.f, 0.f, 1.f }, quat)));
	}
}

