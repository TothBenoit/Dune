#include "pch.h"
#include "MeshLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Mesh.h"

namespace Dune
{
	void ProcessMesh(const aiMesh* pMesh, dVector<Vertex>& vertices, dVector<dU32>& indices, const aiMatrix4x4& transform)
	{
		for (dU32 i = 0; i < pMesh->mNumVertices; i++) 
		{
			dVec3 normal{ (pMesh->HasNormals()) ? dVec3{pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z} : dVec3{0.f,0.f,0.f} };
			aiVector3D& pos = pMesh->mVertices[i];
			pos *= transform;

			vertices.emplace_back(dVec3{ pos.x, pos.y, pos.z }, normal);
		}

		for (dU32 i = 0; i < pMesh->mNumFaces; i++) 
		{
			aiFace& face = pMesh->mFaces[i];
			for (dU32 j = 0; j < face.mNumIndices; j++)
			{
				indices.push_back(face.mIndices[j]);
			}
		}
	}

	void ProcessNode(const aiNode* pNode, const aiScene* pScene, dVector<Handle<Mesh>>& meshes)
	{
		dVector<Vertex> vertices;
		dVector<dU32> indices;

		for (dSizeT i = 0; i < pNode->mNumMeshes; i++)
		{
			vertices.clear();
			indices.clear();

			aiMesh* pMesh{ pScene->mMeshes[pNode->mMeshes[i]] };
			vertices.reserve(pMesh->mNumVertices);

			dSizeT indexCount = 0;
			for (dU32 i = 0; i < pMesh->mNumFaces; i++)
			{
				indexCount += pMesh->mFaces[i].mNumIndices;
			}
			indices.reserve(indexCount);

			ProcessMesh(pMesh, vertices, indices, pNode->mTransformation);
			Handle<Mesh> meshHandle{ Renderer::GetInstance().CreateMesh(indices, vertices) };

			if (meshHandle.IsValid())
			{
				meshes.push_back(meshHandle);
			}
		}

		for (UINT i = 0; i < pNode->mNumChildren; i++)
		{
			ProcessNode(pNode->mChildren[i], pScene, meshes);
		}
	}

	dVector<Handle<Mesh>> MeshLoader::Load(const dString& path)
	{
		dVector<Handle<Mesh>> meshes;
		
		Assimp::Importer importer;
		const aiScene* pScene{ importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded) };
		if (!pScene)
			return meshes;

		const aiNode* pNode{ pScene->mRootNode };
		if (pNode)
			ProcessNode(pNode, pScene, meshes);

		return meshes;
	}

}

