#include "pch.h"
#include "GraphicsComponent.h"

namespace Dune
{
	const dVector<dU32> GraphicsComponent::m_defaultCubeIndices =
	{
		0, 1, 2, 0, 2, 3,
		4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2,
		4, 0, 3, 4, 3, 7
	};

	const dVector<Vertex> GraphicsComponent::m_defaultCubeVertices =
	{
		{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f} }, // 0
		{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f} }, // 1
		{ {0.5f,  0.5f, -0.5f},		{1.0f, 1.0f, 0.0f, 1.0f} }, // 2
		{ {0.5f, -0.5f, -0.5f},		{1.0f, 0.0f, 0.0f, 1.0f} }, // 3
		{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f} }, // 4
		{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f} }, // 5
		{ {0.5f,  0.5f,  0.5f},		{1.0f, 1.0f, 1.0f, 1.0f} }, // 6
		{ {0.5f, -0.5f,  0.5f},		{1.0f, 0.0f, 1.0f, 1.0f} }, // 7
	};

	const std::shared_ptr<Mesh> GraphicsComponent::m_defaultMesh = std::make_shared<Mesh>(Mesh(GraphicsComponent::m_defaultCubeIndices, GraphicsComponent::m_defaultCubeVertices));
}