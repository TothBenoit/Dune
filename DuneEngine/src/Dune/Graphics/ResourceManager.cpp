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
#include <assimp/GltfMaterial.h>

namespace Dune::Graphics
{

	struct NodeRef
	{
		const aiNode* pNode;
		dVec3         position;
		dQuat         rotation;
	};

	static void FlattenNodes(const aiNode* pNode, const aiMatrix4x4& parentTransform, dVector<NodeRef>& outNodes)
	{
		aiMatrix4x4 worldTransform = pNode->mTransformation * parentTransform;

		if (pNode->mNumMeshes > 0)
		{
			aiQuaternion aiRotation;
			aiVector3D   aiPosition;
			worldTransform.DecomposeNoScaling(aiRotation, aiPosition);
			outNodes.push_back({ pNode, { aiPosition.x, aiPosition.y, aiPosition.z }, { aiRotation.x, aiRotation.y, aiRotation.z, aiRotation.w } });
		}

		for (dU32 i = 0; i < pNode->mNumChildren; i++)
			FlattenNodes(pNode->mChildren[i], worldTransform, outNodes);
	}

	static void ExtractMaterial(const aiMaterial* pMaterial, const dString& dirPath, MaterialImportDesc& out)
	{
		out = {};
		aiString texturePath;
		if (pMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == aiReturn_SUCCESS)
			out.albedoPath = dirPath + texturePath.C_Str();
		if (pMaterial->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == aiReturn_SUCCESS)
			out.normalPath = dirPath + texturePath.C_Str();
		if (pMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texturePath) == aiReturn_SUCCESS)
			out.roughnessMetalnessPath = dirPath + texturePath.C_Str();

		aiUVTransform data;
		if (pMaterial->Get(AI_MATKEY_BASE_COLOR, data) == aiReturn_SUCCESS)
			out.baseColor = *((dVec3*)&data);
		if (pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, data) == aiReturn_SUCCESS)
			out.roughnessFactor = *(float*)(&data);
		if (pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, data) == aiReturn_SUCCESS)
			out.metalnessFactor = *(float*)(&data);

		aiString alphaMode;
		if (pMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn_SUCCESS)
		{
			if (strcmp(alphaMode.C_Str(), "MASK") == 0)
				out.alphaMode = EAlphaMode::Mask;
			else if (strcmp(alphaMode.C_Str(), "BLEND") == 0) 
				out.alphaMode = EAlphaMode::Blend;
		}

		float alphaCutoff;
		if (pMaterial->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == aiReturn_SUCCESS)
			out.alphaCutoff = alphaCutoff;

		int doubleSided;
		if (pMaterial->Get(AI_MATKEY_TWOSIDED, doubleSided) == aiReturn_SUCCESS)
			out.doubleSided = doubleSided != 0;
	}

	static void ExtractNodeMesh(const aiNode* pNode, const aiScene* pScene, MeshImportDesc& outMesh, dVector<dU32>& outMaterialIndices)
	{
		if (pNode->mNumMeshes > 0)
		{
			dVector<Vertex>&  vertices = outMesh.vertices;
			dVector<dU32>&    indices = outMesh.indices;
			dVector<SubMesh>& subMeshes = outMesh.subMeshes;
			subMeshes.reserve(pNode->mNumMeshes);

			for (dU32 i = 0; i < pNode->mNumMeshes; i++)
			{
				const aiMesh* pMesh = pScene->mMeshes[pNode->mMeshes[i]];

				dU32 materialSlot;
				for (materialSlot = 0; materialSlot < (dU32)outMaterialIndices.size(); materialSlot++)
					if (outMaterialIndices[materialSlot] == pMesh->mMaterialIndex)
						break;
				if (materialSlot == outMaterialIndices.size())
					outMaterialIndices.push_back(pMesh->mMaterialIndex);

				SubMesh& subMesh = subMeshes.emplace_back();
				subMesh.vertexOffset = (dU32)vertices.size();
				subMesh.indexOffset = (dU32)indices.size();
				subMesh.indexCount = pMesh->mNumFaces * 3;
				subMesh.materialSlot = materialSlot;

				for (dU32 vertexIdx = 0; vertexIdx < pMesh->mNumVertices; vertexIdx++)
				{
					Vertex vertex{};
					const aiVector3D& position = pMesh->mVertices[vertexIdx];
					vertex.position = { position.x, position.y, position.z };

					if (pMesh->HasNormals())
					{
						const aiVector3D& normal = pMesh->mNormals[vertexIdx];
						vertex.normal = { normal.x, normal.y, normal.z };

						if (pMesh->HasTangentsAndBitangents())
						{
							float dotResult;
							const aiVector3D& tangent = pMesh->mTangents[vertexIdx];
							const aiVector3D& bitangent = pMesh->mBitangents[vertexIdx];
							dVec computedBitangent = DirectX::XMVector3Cross({ normal.x, normal.y, normal.z }, { tangent.x, tangent.y, tangent.z });
							DirectX::XMStoreFloat(&dotResult, DirectX::XMVector3Dot(computedBitangent, { bitangent.x, bitangent.y, bitangent.z, }));
							vertex.tangent = { tangent.x, tangent.y, tangent.z, dotResult > 0.0f ? 1.0f : -1.0f };
						}
					}

					if (pMesh->GetNumUVChannels() > 0)
					{
						const aiVector3D& uv = pMesh->mTextureCoords[0][vertexIdx];
						vertex.uv = { uv.x, uv.y };
						vertices.push_back(vertex);
					}
				}
					
				for (dU32 f = 0; f < pMesh->mNumFaces; f++)
				{
					const aiFace& face = pMesh->mFaces[f];
					indices.push_back(face.mIndices[0]);
					indices.push_back(face.mIndices[1]);
					indices.push_back(face.mIndices[2]);
				}
			}
			outMesh.materialSlotCount = (dU32)outMaterialIndices.size();
		}
	}

	void ResourceManager::Initialize(Device& device)
	{
		m_pDevice = &device;
		m_srvHeap.Initialize(device, { .type = EDescriptorHeapType::SRV_CBV_UAV, .capacity = kSharedSRVCapacity, .isShaderVisible = false });
	}

	void ResourceManager::Destroy()
	{
		for (Texture& texture : m_textures)
			texture.Destroy();
		for (dU32 textureID : m_textureIDs)
			m_srvHeap.Free(m_srvHeap.GetDescriptorAt(textureID));
		for (Mesh& mesh : m_meshes)
			mesh.Destroy();
		m_srvHeap.Destroy();
	}

	void ResourceManager::RegisterImageSlot(FileSystem::SerializationID<EFileType::Image> id, dU32 slot)
	{
		if (id.index >= (dU32)m_imageLookup.size())
			m_imageLookup.resize(id.index + 1, dU32(-1));
		m_imageLookup[id.index] = slot;
	}

	dU32 ResourceManager::CreateTextureFromPath(CommandList& commandList, dVector<Buffer>& uploadBuffers, const dString& path, bool sRGB)
	{
		Buffer& uploadBuffer = uploadBuffers.emplace_back();
		Texture texture = DDSTexture::CreateTextureFromFile(*m_pDevice, commandList, uploadBuffer, path.c_str(), sRGB);

		Descriptor textureDescriptor = m_srvHeap.Allocate();
		m_pDevice->CreateSRV(textureDescriptor, texture);

		dU32 slot = (dU32)m_textures.size();
		m_textures.push_back(texture);
		m_textureIDs.push_back(m_srvHeap.GetIndex(textureDescriptor));
		return slot;
	}

	dU32 ResourceManager::ResolveTexture(const dString& path, CommandList& commandList, dVector<Buffer>& uploadBuffers, dVector<dU32>& newTextureSlots, bool sRGB)
	{
		if (path.empty())
			return dU32(-1);
		FileSystem::SerializationID<EFileType::Image> id = FileSystem::Resolve<EFileType::Image>(path.c_str());
		dU32 slot = id.index < (dU32)m_imageLookup.size() ? m_imageLookup[id.index] : dU32(-1);
		if (slot == dU32(-1))
		{
			slot = CreateTextureFromPath(commandList, uploadBuffers, path, sRGB);
			newTextureSlots.push_back(slot);
			RegisterImageSlot(id, slot);
		}
		return slot;
	};

	dU32 ResourceManager::CreateMaterial(const MaterialImportDesc& desc, CommandList& commandList, dVector<Buffer>& uploadBuffers, dVector<dU32>& newTextureSlots)
	{
		Material material
		{
			.shaderData =
			{
				.baseColor = desc.baseColor,
				.metalnessFactor = desc.metalnessFactor,
				.roughnessFactor = desc.roughnessFactor,
				.albedoIdx = ResolveTexture(desc.albedoPath, commandList, uploadBuffers, newTextureSlots, true),
				.normalIdx = ResolveTexture(desc.normalPath, commandList, uploadBuffers, newTextureSlots, false),
				.roughnessMetalnessIdx = ResolveTexture(desc.roughnessMetalnessPath, commandList, uploadBuffers, newTextureSlots, false),
				.alphaCutoff = desc.alphaCutoff
			},
			.alphaMode = desc.alphaMode,
			.isDoubleSided = desc.doubleSided
		};

		if (material.shaderData.albedoIdx != dU32(-1))
			material.shaderData.albedoIdx = m_textureIDs[material.shaderData.albedoIdx];
		if (material.shaderData.normalIdx != dU32(-1))
			material.shaderData.normalIdx = m_textureIDs[material.shaderData.normalIdx];
		if (material.shaderData.roughnessMetalnessIdx != dU32(-1))
			material.shaderData.roughnessMetalnessIdx = m_textureIDs[material.shaderData.roughnessMetalnessIdx];

		dU32 slot = (dU32)m_materials.size();
		m_materials.push_back(material);
		return slot;
	}

	dU32 ResourceManager::CreateMesh(const MeshImportDesc& importDesc, CommandList& commandList, dVector<Buffer>& uploadBuffers)
	{
		Mesh mesh{};
		Buffer& uploadBuffer = uploadBuffers.emplace_back();
		MeshDesc desc
		{
			.pVertices = importDesc.vertices.data(),
			.vertexCount = (dU32)importDesc.vertices.size(),
			.vertexByteStride = sizeof(Vertex),
			.pIndices = importDesc.indices.data(),
			.indexCount = (dU32)importDesc.indices.size(),
			.isIndex32bits = true,
			.pSubMeshes = importDesc.subMeshes.data(),
			.subMeshCount = (dU32)importDesc.subMeshes.size(),
			.materialSlotCount = importDesc.materialSlotCount
		};
		mesh.Initialize(*m_pDevice, commandList, uploadBuffer, desc);
		m_meshes.push_back(mesh);
		return (dU32)m_meshes.size() - 1;
	}

	dU32 ResourceManager::GetTexture(FileSystem::SerializationID<EFileType::Image> id, bool sRGB)
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
		barrier.PushTransition(m_textures[slot], EResourceState::CopyDest, EResourceState::ShaderResource);
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

	const ModelData& ResourceManager::GetModel(FileSystem::SerializationID<EFileType::Model> id)
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

		dVector<Buffer> uploadBuffers;
		dVector<dU32>   newTextureSlots;

		dVector<NodeRef> nodeRefs;
		FlattenNodes(pScene->mRootNode, {}, nodeRefs);

		constexpr dU32 kInvalidMaterialIdx = dU32(-1);
		dVector<dU32> materialLookup(pScene->mNumMaterials, kInvalidMaterialIdx);

		MeshImportDesc meshDesc;
		dVector<dU32>  nodeMaterials;
		for (const NodeRef& ref : nodeRefs)
		{
			meshDesc.vertices.clear();
			meshDesc.indices.clear();
			meshDesc.subMeshes.clear();
			nodeMaterials.clear();
			ExtractNodeMesh(ref.pNode, pScene, meshDesc, nodeMaterials);

			if (nodeMaterials.size() == 0)
				continue;

			ModelNode& node = outModel.nodes.emplace_back();
			node.position = ref.position;
			node.rotation = ref.rotation;
			node.meshIndex = CreateMesh(meshDesc, commandList, uploadBuffers);
			node.materialSlotStart = (dU32)m_materialIDs.size();
			node.materialSlotCount = (dU32)nodeMaterials.size();

			for (dU32 aiMaterialIdx : nodeMaterials)
			{
				dU32 materialIndex = materialLookup[aiMaterialIdx];
				if (materialIndex == kInvalidMaterialIdx)
				{
					MaterialImportDesc materialDesc;
					ExtractMaterial(pScene->mMaterials[aiMaterialIdx], dirPath, materialDesc);
					materialLookup[aiMaterialIdx] = materialIndex = CreateMaterial(materialDesc, commandList, uploadBuffers, newTextureSlots);
				}
				m_materialIDs.push_back(materialIndex);
			}
		}

		Barrier barrier{};
		barrier.Initialize((dU32)newTextureSlots.size());
		for (dU32 slot : newTextureSlots)
			barrier.PushTransition(m_textures[slot], EResourceState::CopyDest, EResourceState::ShaderResource);
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
