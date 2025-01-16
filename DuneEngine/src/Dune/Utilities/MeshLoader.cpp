#include "pch.h"
#include "Dune/Utilities/MeshLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Dune/Graphics/Mesh.h"

namespace Dune::Graphics
{
	//Device* g_pCurrentDevice{ nullptr };

	//void ProcessMesh(const aiMesh* pMesh, dVector<Vertex>& vertices, dVector<dU32>& indices, const aiMatrix4x4& transform)
	//{
	//	for (dU32 i = 0; i < pMesh->mNumVertices; i++) 
	//	{
	//		dVec3 normal{ (pMesh->HasNormals()) ? dVec3{pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z} : dVec3{0.f,0.f,0.f} };
	//		aiVector3D& pos = pMesh->mVertices[i];
	//		pos *= transform;

	//		vertices.emplace_back(dVec3{ pos.x, pos.y, pos.z }, normal);
	//	}

	//	for (dU32 i = 0; i < pMesh->mNumFaces; i++) 
	//	{
	//		aiFace& face = pMesh->mFaces[i];
	//		for (dU32 j = 0; j < face.mNumIndices; j++)
	//		{
	//			indices.push_back(face.mIndices[j]);
	//		}
	//	}
	//}

	//void ProcessNode(const aiNode* pNode, const aiScene* pScene, dVector<Mesh*>& meshes)
	//{
	//	dVector<Vertex> vertices;
	//	dVector<dU32> indices;

	//	for (dSizeT i = 0; i < pNode->mNumMeshes; i++)
	//	{
	//		vertices.clear();
	//		indices.clear();

	//		aiMesh* pAiMesh{ pScene->mMeshes[pNode->mMeshes[i]] };
	//		vertices.reserve(pAiMesh->mNumVertices);

	//		dSizeT indexCount = 0;
	//		for (dU32 i = 0; i < pAiMesh->mNumFaces; i++)
	//		{
	//			indexCount += pAiMesh->mFaces[i].mNumIndices;
	//		}
	//		indices.reserve(indexCount);

	//		ProcessMesh(pAiMesh, vertices, indices, pNode->mTransformation);
	//		Mesh* pMesh = new Mesh();
	//		pMesh->Initialize(g_pCurrentDevice, indices.data(), (dU32)indices.size(), vertices.data(), (dU32)vertices.size(), sizeof(Vertex));

	//		meshes.push_back(pMesh);
	//	}

	//	for (dU32 i = 0; i < pNode->mNumChildren; i++)
	//	{
	//		ProcessNode(pNode->mChildren[i], pScene, meshes);
	//	}
	//}

	//dVector<Mesh*> MeshLoader::Load(Device* pDevice, const char* path)
	//{
	//	Assert(pDevice);
	//	g_pCurrentDevice = pDevice;
	//	dVector<Mesh*> meshes;
	//	
	//	Assimp::Importer importer;
	//	const aiScene* pScene{ importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded) };
	//	if (!pScene)
	//		return meshes;

	//	const aiNode* pNode{ pScene->mRootNode };
	//	if (pNode)
	//		ProcessNode(pNode, pScene, meshes);

	//	return meshes;
	//}

}