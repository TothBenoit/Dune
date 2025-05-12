#include "pch.h"
#include "Dune/Utilities/SceneLoader.h"
#include "Dune/Scene/Scene.h"
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/RHI/Buffer.h>
#include <Dune/Graphics/RHI/Device.h>
#include <Dune/Graphics/RHI/Fence.h>
#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/CommandList.h>
#include <Dune/Graphics/Shaders/ShaderTypes.h>
#include <Dune/Graphics/Mesh.h>
#include <Dune/Utilities/DDSLoader.h>
#include <Dune/Core/Logger.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Dune::SceneLoader
{
	void ProcessNode(const aiNode* pNode, Scene& scene, const aiMatrix4x4& parentTransform )
	{
		aiMatrix4x4 worldTransform = pNode->mTransformation * parentTransform;
		aiQuaternion aiRotation;
		aiVector3D aiPosition;
		worldTransform.DecomposeNoScaling(aiRotation, aiPosition);

		for (dSizeT i = 0; i < pNode->mNumMeshes; i++)
		{
			dU32 meshIdx = pNode->mMeshes[i];
			EntityID entity = scene.registry.create();

			Name& name = scene.registry.emplace<Name>(entity);
			name.name.assign("Mesh");
			Transform& transform = scene.registry.emplace<Transform>(entity);
			transform.position = { aiPosition.x, aiPosition.y, aiPosition.z };
			transform.rotation = { aiRotation.x, aiRotation.y, aiRotation.z, aiRotation.w };
			transform.scale = 1.0f;
			RenderData& renderData = scene.registry.emplace<RenderData>(entity);
			renderData.meshIdx = meshIdx;
			renderData.materialIdx = meshIdx;
		}

		dU32 childCount = pNode->mNumChildren;
		for (dU32 i = 0; i < childCount; i++)
			ProcessNode(pNode->mChildren[i], scene, worldTransform);
	}

	bool Load(const char* dirPath, const char* fileName, Scene& scene, Graphics::Device& device) 
	{
		Assimp::Importer importer;
		dString path{ dirPath };
		path.append(fileName);
		const aiScene* pScene{ importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace) };
		if (!pScene) {
			LOG_ERROR(importer.GetErrorString());
			return false;
		}

		Graphics::ECommandType commandType = Graphics::ECommandType::Direct;
		Graphics::CommandQueue commandQueue;
		commandQueue.Initialize(&device, commandType);
		Graphics::CommandAllocator commandAllocator;
		commandAllocator.Initialize(&device, commandType);
		Graphics::CommandList commandList;
		commandList.Initialize(&device, commandType, commandAllocator);
		commandList.Close();
		commandAllocator.Reset();
		commandList.Reset(commandAllocator);

		dU32 meshCount = pScene->mNumMeshes;
		scene.meshes.resize(meshCount + scene.meshes.size());

		dU32 prevTextureCount = (dU32)scene.textures.size();
		dVector<Graphics::Buffer> uploadBuffers;

		for (dU32 meshIdx = 0; meshIdx < meshCount; meshIdx++)
		{
			const aiMesh* pMesh = pScene->mMeshes[meshIdx];
			dU32 vertexCount = pMesh->mNumVertices;
			dVector<Graphics::Vertex> vertices(vertexCount);
			for (dU32 vertexIdx = 0; vertexIdx < vertexCount; vertexIdx++)
			{
				Graphics::Vertex& vertex{ vertices[vertexIdx] };
				const aiVector3D& position = pMesh->mVertices[vertexIdx];
				vertex.position = { position.x, position.y, position.z };
					
				const aiVector3D& normal = pMesh->mNormals[vertexIdx];
				vertex.normal = { normal.x, normal.y, normal.z };

				const aiVector3D& tangent = pMesh->mTangents[vertexIdx];
				vertex.tangent = { tangent.x, tangent.y, tangent.z };

				const aiVector3D& uv = pMesh->mTextureCoords[0][vertexIdx];
				vertex.uv = { uv.x, uv.y };
			}

			dU32 faceCount = pMesh->mNumFaces;
			dU32 indexCount = faceCount * 3;
			dVector<dU32> indices(indexCount);
			dU32 index = 0;
			for (dU32 faceIdx = 0; faceIdx < faceCount; faceIdx++)
			{
				const aiFace& face = pMesh->mFaces[faceIdx];
				for (dU32 i = 0; i < 3; i++)
					indices[index++] = face.mIndices[i];
			}

			Graphics::Mesh& mesh = scene.meshes[meshIdx];
			mesh.Initialize(&device, &commandList, indices.data(), (dU32)indices.size(), vertices.data(), (dU32)vertices.size(), sizeof(Graphics::Vertex));

			dU32 materialIdx = pMesh->mMaterialIndex;
			const aiMaterial* pMaterial = pScene->mMaterials[materialIdx];
			{
				aiString texturePath;
				pMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);
				dString path{ dirPath };
				path.append(texturePath.length == 0 ? "defaultAlbedo.DDS" : texturePath.C_Str());
				Graphics::Buffer& uploadBuffer = uploadBuffers.emplace_back();
				scene.textures.push_back(Graphics::DDSTexture::CreateTextureFromFile(&device, &commandList, uploadBuffer, path.c_str(), true));
			}

			{
				aiString texturePath;
				pMaterial->GetTexture(aiTextureType_NORMALS, 0, &texturePath);
				Graphics::Buffer& uploadBuffer = uploadBuffers.emplace_back();
				dString path{ dirPath };
				path.append( texturePath.length == 0 ? "defaultNormal.DDS" : texturePath.C_Str());
				scene.textures.push_back(Graphics::DDSTexture::CreateTextureFromFile(&device, &commandList, uploadBuffer, path.c_str()));
			}

			{
				aiString texturePath;
				pMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texturePath);
				Graphics::Buffer& uploadBuffer = uploadBuffers.emplace_back();
				dString path{ dirPath };
				path.append(texturePath.length == 0 ? "defaultRoughnessMetalness.DDS" : texturePath.C_Str());
				scene.textures.push_back(Graphics::DDSTexture::CreateTextureFromFile(&device, &commandList, uploadBuffer, path.c_str()));
			}

			Graphics::MaterialData material;
			aiUVTransform data;
			pMaterial->Get(AI_MATKEY_BASE_COLOR, data);
			material.baseColor = *((dVec3*)&data);
			pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, data);
			material.roughnessFactor = *(float*)(&data);
			pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, data);
			material.metalnessFactor = *(float*)(&data);
			material.albedoIdx = meshIdx * 3;
			material.normalIdx = meshIdx * 3 + 1;
			material.roughnessMetalnessIdx = meshIdx * 3 + 2;
			scene.materials.push_back(material);
		}

		const aiNode* pCurNode = pScene->mRootNode;
		ProcessNode(pCurNode, scene, {});

		dU32 textureCount = (dU32)scene.textures.size();
		Graphics::Barrier barrier{};
		barrier.Initialize(textureCount - prevTextureCount);
		for (dU32 textureIdx = prevTextureCount; textureIdx < textureCount; textureIdx++)
			barrier.PushTransition(scene.textures[textureIdx].Get(), Graphics::EResourceState::CopyDest, Graphics::EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Destroy();
		commandList.Close();
		commandQueue.ExecuteCommandLists(&commandList, 1);

		Graphics::Fence fence;
		fence.Initialize(&device, 0);
		commandQueue.Signal(fence, 1);
		fence.Wait(1);
		fence.Destroy();
		commandQueue.Destroy();
		commandAllocator.Destroy();
		commandList.Destroy();

		for (Graphics::Buffer& buffer : uploadBuffers)
			buffer.Destroy();
		
		return true;
	}
}