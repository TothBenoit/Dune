#include "pch.h"
#include "Dune/Graphics/MeshUtils.h"
#include <cmath>

namespace Dune::MeshUtils
{
	using Graphics::MeshData;
	using Graphics::Vertex;

	static float ComputeTangentSign(dVec normal, dVec tangent, dVec bitangent)
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(normal, tangent), bitangent)) >= 0.0f ? 1.0f : -1.0f;
	}

	static void PushTriangle(MeshData& meshData, dU32 a, dU32 b, dU32 c)
	{
		const Vertex& va = meshData.vertices[a];
		const Vertex& vb = meshData.vertices[b];
		const Vertex& vc = meshData.vertices[c];
		const dVec pa = DirectX::XMLoadFloat3(&va.position);
		const dVec faceNormal = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&vb.position), pa), DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&vc.position), pa));
		const dVec vertexNormal = DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&va.normal), DirectX::XMLoadFloat3(&vb.normal)), DirectX::XMLoadFloat3(&vc.normal));
		const bool isFrontFacing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(faceNormal, vertexNormal)) >= 0.0f;

		meshData.indices.push_back(a);
		meshData.indices.push_back(isFrontFacing ? b : c);
		meshData.indices.push_back(isFrontFacing ? c : b);
	}

	static void AddQuad(MeshData& meshData, dVec center, dVec normal, dVec halfRight, dVec halfDown)
	{
		const dU32 base = (dU32)meshData.vertices.size();
		const dVec tangent = DirectX::XMVector3Normalize(halfRight);
		const float tangentSign = ComputeTangentSign(normal, tangent, halfDown);

		const float corners[4][2]{ { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		for (const float (&corner)[2] : corners)
		{
			Vertex& vertex = meshData.vertices.emplace_back();
			const dVec position = DirectX::XMVectorAdd(center, DirectX::XMVectorAdd(DirectX::XMVectorScale(halfRight, corner[0]), DirectX::XMVectorScale(halfDown, corner[1])));
			DirectX::XMStoreFloat3(&vertex.position, position);
			DirectX::XMStoreFloat3(&vertex.normal, normal);
			DirectX::XMStoreFloat4(&vertex.tangent, DirectX::XMVectorSetW(tangent, tangentSign));
			vertex.uv = { corner[0] * 0.5f + 0.5f, corner[1] * 0.5f + 0.5f };
		}

		PushTriangle(meshData, base + 0, base + 1, base + 2);
		PushTriangle(meshData, base + 0, base + 2, base + 3);
	}

	static void Finalize(MeshData& meshData)
	{
		meshData.subMeshes.push_back({ .indexOffset = 0, .indexCount = (dU32)meshData.indices.size(), .vertexOffset = 0, .materialSlot = 0 });
		meshData.materialSlotCount = 1;
	}

	MeshData CreateCube(const dVec3& size)
	{
		struct Face
		{
			dVec normal;
			dVec up;
		};

		const Face faces[]
		{
			{ {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f } },
			{ { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f } },
			{ {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f,  0.0f } },
			{ {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f,  0.0f } },
			{ {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f,  1.0f } },
			{ {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f, -1.0f } },
		};

		const dVec halfSize = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&size), 0.5f);

		MeshData meshData;
		meshData.vertices.reserve(24);
		meshData.indices.reserve(36);
		for (const Face& face : faces)
		{
			const dVec right = DirectX::XMVector3Cross(face.up, DirectX::XMVectorNegate(face.normal));
			AddQuad(meshData,
				DirectX::XMVectorMultiply(face.normal, halfSize),
				face.normal,
				DirectX::XMVectorMultiply(right, halfSize),
				DirectX::XMVectorMultiply(DirectX::XMVectorNegate(face.up), halfSize));
		}
		Finalize(meshData);
		return meshData;
	}

	MeshData CreatePlane(const dVec2& size)
	{
		MeshData meshData;
		AddQuad(meshData,
			DirectX::XMVectorZero(),
			{ 0.0f, 1.0f, 0.0f },
			{ size.x * 0.5f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, -size.y * 0.5f });
		Finalize(meshData);
		return meshData;
	}

	MeshData CreateSphere(float radius, dU32 segmentCount)
	{
		const dU32 segments = std::max(segmentCount, 3u);
		const dU32 rings = std::max(segments / 2, 2u);

		MeshData meshData;
		meshData.vertices.reserve((rings + 1) * (segments + 1));
		for (dU32 ring = 0; ring <= rings; ring++)
		{
			const float theta = DirectX::XM_PI * (float)ring / (float)rings;
			const float sinTheta = std::sin(theta);
			const float cosTheta = std::cos(theta);
			for (dU32 segment = 0; segment <= segments; segment++)
			{
				const float phi = DirectX::XM_2PI * (float)segment / (float)segments;
				const float sinPhi = std::sin(phi);
				const float cosPhi = std::cos(phi);

				const dVec normal{ sinTheta * cosPhi, cosTheta, sinTheta * sinPhi };
				const dVec tangent{ -sinPhi, 0.0f, cosPhi };
				const dVec bitangent{ cosTheta * cosPhi, -sinTheta, cosTheta * sinPhi };

				Vertex& vertex = meshData.vertices.emplace_back();
				DirectX::XMStoreFloat3(&vertex.position, DirectX::XMVectorScale(normal, radius));
				DirectX::XMStoreFloat3(&vertex.normal, normal);
				DirectX::XMStoreFloat4(&vertex.tangent, DirectX::XMVectorSetW(tangent, ComputeTangentSign(normal, tangent, bitangent)));
				vertex.uv = { (float)segment / (float)segments, (float)ring / (float)rings };
			}
		}

		const dU32 stride = segments + 1;
		for (dU32 ring = 0; ring < rings; ring++)
		{
			for (dU32 segment = 0; segment < segments; segment++)
			{
				const dU32 topLeft = ring * stride + segment;
				const dU32 topRight = topLeft + 1;
				const dU32 bottomLeft = topLeft + stride;
				const dU32 bottomRight = bottomLeft + 1;
				if (ring != 0)
					PushTriangle(meshData, topLeft, topRight, bottomRight);
				if (ring != rings - 1)
					PushTriangle(meshData, topLeft, bottomRight, bottomLeft);
			}
		}
		Finalize(meshData);
		return meshData;
	}

	MeshData CreatePrimitive(EPrimitive primitive)
	{
		switch (primitive)
		{
		case EPrimitive::Cube:
			return CreateCube();
		case EPrimitive::Sphere:
			return CreateSphere();
		case EPrimitive::Plane:
			return CreatePlane();
		default:
			Assert(false);
			return {};
		}
	}

	void Transform(MeshData& meshData, const dMatrix& matrix)
	{
		const dMatrix normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, matrix));
		const bool isMirrored = DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(matrix)) < 0.0f;

		for (Vertex& vertex : meshData.vertices)
		{
			DirectX::XMStoreFloat3(&vertex.position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex.position), matrix));
			DirectX::XMStoreFloat3(&vertex.normal, DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&vertex.normal), normalMatrix)));
			const dVec tangent = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&vertex.tangent), matrix));
			DirectX::XMStoreFloat4(&vertex.tangent, DirectX::XMVectorSetW(tangent, isMirrored ? -vertex.tangent.w : vertex.tangent.w));
		}

		if (isMirrored)
		{
			for (dSizeT i = 0; i + 2 < meshData.indices.size(); i += 3)
				std::swap(meshData.indices[i + 1], meshData.indices[i + 2]);
		}
	}

	void Append(MeshData& destination, const MeshData& source)
	{
		const dU32 vertexOffset = (dU32)destination.vertices.size();
		const dU32 indexOffset = (dU32)destination.indices.size();
		const dU32 materialSlotOffset = destination.materialSlotCount;

		destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
		destination.indices.insert(destination.indices.end(), source.indices.begin(), source.indices.end());
		for (Graphics::SubMesh subMesh : source.subMeshes)
		{
			subMesh.vertexOffset += vertexOffset;
			subMesh.indexOffset += indexOffset;
			subMesh.materialSlot += materialSlotOffset;
			destination.subMeshes.push_back(subMesh);
		}
		destination.materialSlotCount += source.materialSlotCount;
	}
}
