#include "pch.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/Fence.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Utilities/DDSLoader.h"
#include "Dune/Core/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Dune::Graphics
{
	static void ImportNode(const aiNode* pNode, ModelData& outModel, const dVector<dU32>& meshSlots, const dVector<dU32>& materialSlots, const aiMatrix4x4& parentTransform)
	{
		aiMatrix4x4 worldTransform = pNode->mTransformation * parentTransform;
		aiQuaternion aiRotation;
		aiVector3D aiPosition;
		worldTransform.DecomposeNoScaling(aiRotation, aiPosition);

		for (dU32 i = 0; i < pNode->mNumMeshes; i++)
		{
			dU32 meshIdx = pNode->mMeshes[i];
			ModelNode& node = outModel.nodes.emplace_back();
			node.position = { aiPosition.x, aiPosition.y, aiPosition.z };
			node.rotation = { aiRotation.x, aiRotation.y, aiRotation.z, aiRotation.w };
			node.meshIndex = meshSlots[meshIdx];
			node.materialIndex = materialSlots[meshIdx];
		}

		for (dU32 i = 0; i < pNode->mNumChildren; i++)
			ImportNode(pNode->mChildren[i], outModel, meshSlots, materialSlots, worldTransform);
	}

	void ResourceManager::Initialize(Device& device)
	{
		m_pDevice = &device;
	}

	void ResourceManager::Destroy()
	{
		for (Texture& texture : m_textures)
			texture.Destroy();
		for (Mesh& mesh : m_meshes)
			mesh.Destroy();
	}

	void ResourceManager::RegisterImageSlot(FileSystem::SerializationID<EResourceType::Image> id, dU32 slot)
	{
		if (id.index >= (dU32)m_imageLookup.size())
			m_imageLookup.resize(id.index + 1, dU32(-1));
		m_imageLookup[id.index] = slot;
	}

	dU32 ResourceManager::CreateTextureFromPath(CommandList& commandList, dVector<Buffer>& uploadBuffers, const dString& path, bool sRGB)
	{
		Buffer& uploadBuffer = uploadBuffers.emplace_back();
		Texture texture = DDSTexture::CreateTextureFromFile(*m_pDevice, commandList, uploadBuffer, path.c_str(), sRGB);
		dU32 slot = (dU32)m_textures.size();
		m_textures.push_back(texture);
		return slot;
	}

	dU32 ResourceManager::GetTexture(FileSystem::SerializationID<EResourceType::Image> id, bool sRGB)
	{
		if (id.index < (dU32)m_imageLookup.size() && m_imageLookup[id.index] != dU32(-1))
			return m_imageLookup[id.index];

		CommandQueue commandQueue;
		commandQueue.Initialize(*m_pDevice, ECommandType::Direct);
		CommandAllocator commandAllocator;
		commandAllocator.Initialize(*m_pDevice, ECommandType::Direct);
		CommandList commandList;
		commandList.Initialize(*m_pDevice, ECommandType::Direct, commandAllocator);
		commandList.Close();
		commandAllocator.Reset();
		commandList.Reset(commandAllocator);

		dVector<Buffer> uploadBuffers;
		dU32 slot = CreateTextureFromPath(commandList, uploadBuffers, FileSystem::GetPath(id), sRGB);
		RegisterImageSlot(id, slot);

		Barrier barrier{};
		barrier.Initialize(1);
		barrier.PushTransition(m_textures[slot].Get(), EResourceState::CopyDest, EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Destroy();
		commandList.Close();
		commandQueue.ExecuteCommandLists(&commandList, 1);

		Fence fence;
		fence.Initialize(*m_pDevice, 0);
		commandQueue.Signal(fence, 1);
		fence.Wait(1);
		fence.Destroy();

		for (Buffer& buffer : uploadBuffers)
			buffer.Destroy();
		commandQueue.Destroy();
		commandAllocator.Destroy();
		commandList.Destroy();

		return slot;
	}

	const ModelData& ResourceManager::GetModel(FileSystem::SerializationID<EResourceType::Model> id)
	{
		if (id.index >= (dU32)m_modelLookup.size())
			m_modelLookup.resize(id.index + 1, dU32(-1));

		dU32& slot = m_modelLookup[id.index];
		if (slot == dU32(-1))
		{
			slot = (dU32)m_models.size();
			m_models.emplace_back();
			ImportModel(FileSystem::GetPath(id), m_models.back());
		}
		return m_models[slot];
	}

	void ResourceManager::ImportModel(const dString& path, ModelData& outModel)
	{
		Assimp::Importer importer;
		const aiScene* pScene{ importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace) };
		if (!pScene)
		{
			LOG_ERROR(importer.GetErrorString());
			return;
		}

		dSizeT lastSlash = path.find_last_of("/\\");
		dString dirPath = (lastSlash == dString::npos) ? dString() : path.substr(0, lastSlash + 1);

		CommandQueue commandQueue;
		commandQueue.Initialize(*m_pDevice, ECommandType::Direct);
		CommandAllocator commandAllocator;
		commandAllocator.Initialize(*m_pDevice, ECommandType::Direct);
		CommandList commandList;
		commandList.Initialize(*m_pDevice, ECommandType::Direct, commandAllocator);
		commandList.Close();
		commandAllocator.Reset();
		commandList.Reset(commandAllocator);

		dU32 meshCount = pScene->mNumMeshes;
		dVector<dU32> meshSlots(meshCount);
		dVector<dU32> materialSlots(meshCount);
		dVector<Buffer> uploadBuffers;
		dVector<dU32> newTextureSlots;

		for (dU32 meshIdx = 0; meshIdx < meshCount; meshIdx++)
		{
			const aiMesh* pMesh = pScene->mMeshes[meshIdx];
			dU32 vertexCount = pMesh->mNumVertices;
			dVector<Vertex> vertices(vertexCount);
			for (dU32 vertexIdx = 0; vertexIdx < vertexCount; vertexIdx++)
			{
				Vertex& vertex{ vertices[vertexIdx] };
				const aiVector3D& position = pMesh->mVertices[vertexIdx];
				vertex.position = { position.x, position.y, position.z };

				const aiVector3D& normal = pMesh->mNormals[vertexIdx];
				vertex.normal = { normal.x, normal.y, normal.z };

				float dotResult;
				const aiVector3D& tangent = pMesh->mTangents[vertexIdx];
				const aiVector3D& bitangent = pMesh->mBitangents[vertexIdx];
				dVec computedBitangent = DirectX::XMVector3Cross({ normal.x, normal.y, normal.z }, { tangent.x, tangent.y, tangent.z });
				DirectX::XMStoreFloat(&dotResult, DirectX::XMVector3Dot(computedBitangent, { bitangent.x, bitangent.y, bitangent.z, }));
				vertex.tangent = { tangent.x, tangent.y, tangent.z, dotResult > 0.0f ? 1.0f : -1.0f };

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

			Mesh mesh{};
			mesh.Initialize(*m_pDevice, commandList, indices.data(), (dU32)indices.size(), vertices.data(), (dU32)vertices.size(), sizeof(Vertex));
			meshSlots[meshIdx] = (dU32)m_meshes.size();
			m_meshes.push_back(mesh);

			MaterialData material
			{
				.baseColor = { 1.0f, 1.0f, 1.0f },
				.metalnessFactor = 1.0f,
				.roughnessFactor = 1.0f,
				.albedoIdx = dU32(-1),
				.normalIdx = dU32(-1),
				.roughnessMetalnessIdx = dU32(-1),
			};

			const aiMaterial* pMaterial = pScene->mMaterials[pMesh->mMaterialIndex];
			{
				aiString texturePath;
				if (pMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == aiReturn_SUCCESS)
				{
					dString path = dirPath + texturePath.C_Str();
					dU32 slot = CreateTextureFromPath(commandList, uploadBuffers, path, true);
					newTextureSlots.push_back(slot);
					RegisterImageSlot(FileSystem::Resolve<EResourceType::Image>(path.c_str()), slot);
					material.albedoIdx = slot;
				}
			}
			{
				aiString texturePath;
				if (pMaterial->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == aiReturn_SUCCESS)
				{
					dString path = dirPath + texturePath.C_Str();
					dU32 slot = CreateTextureFromPath(commandList, uploadBuffers, path, false);
					newTextureSlots.push_back(slot);
					RegisterImageSlot(FileSystem::Resolve<EResourceType::Image>(path.c_str()), slot);
					material.normalIdx = slot;
				}
			}
			{
				aiString texturePath;
				if (pMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texturePath) == aiReturn_SUCCESS)
				{
					dString path = dirPath + texturePath.C_Str();
					dU32 slot = CreateTextureFromPath(commandList, uploadBuffers, path, false);
					newTextureSlots.push_back(slot);
					RegisterImageSlot(FileSystem::Resolve<EResourceType::Image>(path.c_str()), slot);
					material.roughnessMetalnessIdx = slot;
				}
			}

			aiUVTransform data;
			if (pMaterial->Get(AI_MATKEY_BASE_COLOR, data) == aiReturn_SUCCESS)
				material.baseColor = *((dVec3*)&data);
			if (pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, data) == aiReturn_SUCCESS)
				material.roughnessFactor = *(float*)(&data);
			if (pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, data) == aiReturn_SUCCESS)
				material.metalnessFactor = *(float*)(&data);

			materialSlots[meshIdx] = (dU32)m_materials.size();
			m_materials.push_back(material);
		}

		ImportNode(pScene->mRootNode, outModel, meshSlots, materialSlots, {});

		Barrier barrier{};
		barrier.Initialize((dU32)newTextureSlots.size());
		for (dU32 slot : newTextureSlots)
			barrier.PushTransition(m_textures[slot].Get(), EResourceState::CopyDest, EResourceState::ShaderResource);
		commandList.Transition(barrier);
		barrier.Destroy();
		commandList.Close();
		commandQueue.ExecuteCommandLists(&commandList, 1);

		Fence fence;
		fence.Initialize(*m_pDevice, 0);
		commandQueue.Signal(fence, 1);
		fence.Wait(1);
		fence.Destroy();
		commandQueue.Destroy();
		commandAllocator.Destroy();
		commandList.Destroy();

		for (Buffer& buffer : uploadBuffers)
			buffer.Destroy();
	}
}
