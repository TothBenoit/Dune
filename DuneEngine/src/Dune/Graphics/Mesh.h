#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Buffer;

	struct Vertex
	{
		dVec3 position;
		dVec4 color;
		dVec3 normal;
	};

	class Mesh
	{
		DISABLE_COPY_AND_MOVE(Mesh);
	public:

		[[nodiscard]] Handle<Buffer> GetIndexBufferHandle() const { return m_indexBufferHandle; }
		[[nodiscard]] Handle<Buffer> GeVertexBufferHandle() const { return m_vertexBufferHandle; }

	private:
		Mesh(const dVector<dU32>& indices = defaultCubeIndices, const dVector<Vertex>& vertices = defaultCubeVertices);
		~Mesh();

		void UploadBuffers();
		void UploadVertexBuffer();
		void UploadIndexBuffer();

	private:
		template <typename T, typename H> friend class Pool;

		dVector<dU32> m_indices;
		dVector<Vertex> m_vertices;

		Handle<Buffer> m_indexBufferHandle;
		Handle<Buffer> m_vertexBufferHandle;

		inline static const dVector<dU32> defaultCubeIndices
		{
			0, 1, 2, 0, 2, 3,			//Face
			4, 6, 5, 4, 7, 6,			//Back
			10, 11, 9, 10, 9, 8,		//Left
			13, 12, 14, 13, 14, 15,		//Right
			16, 18, 19, 16, 19, 17,		//Top
			22, 20, 21, 22, 21, 23		//Bottom
		};

		inline static const dVector<Vertex> defaultCubeVertices
		{
			{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 0
			{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 1
			{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 2
			{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, 0.0f, -1.0f } }, // 3

			{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 4
			{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 5
			{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 6
			{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, 0.0f,  1.0f } }, // 7

			{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 8
			{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 9
			{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 10
			{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{-1.0f, 0.0f,  0.0f } }, // 11

			{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 12
			{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 13
			{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 14
			{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 1.0f, 0.0f,  0.0f } }, // 15

			{ {-0.5f,  0.5f, -0.5f},	{0.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 16
			{ { 0.5f,  0.5f, -0.5f},	{1.0f, 1.0f, 0.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 17
			{ {-0.5f,  0.5f,  0.5f},	{0.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 18
			{ { 0.5f,  0.5f,  0.5f},	{1.0f, 1.0f, 1.0f, 1.0f},	{ 0.0f, 1.0f, 0.0f } }, // 19

			{ {-0.5f, -0.5f, -0.5f},	{0.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 20
			{ { 0.5f, -0.5f, -0.5f},	{1.0f, 0.0f, 0.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 21
			{ {-0.5f, -0.5f,  0.5f},	{0.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 22
			{ { 0.5f, -0.5f,  0.5f},	{1.0f, 0.0f, 1.0f, 1.0f},	{ 0.0f, -1.0f, 0.0f } }, // 23
		};

	};
}