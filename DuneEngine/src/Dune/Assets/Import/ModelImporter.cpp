#include "pch.h"
#include "Dune/Assets/Import/ModelImporter.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Platform/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>

namespace Dune::ModelImporter
{
	namespace
	{
		struct NodeRef
		{
			const aiNode* pNode;
			dVec3         position;
			dVec4         rotation;
			aiVector3D    scale;
		};

		struct ImportContext
		{
			const aiScene*          pScene{ nullptr };
			dString                 directory;
			ModelData*              pModel{ nullptr };
			dHashMap<dString, dU32> imageLookup;
			dVector<dU32>           materialLookup;
		};

		void FlattenNodes(const aiNode* pNode, const aiMatrix4x4& parentTransform, dVector<NodeRef>& outNodes)
		{
			aiMatrix4x4 worldTransform = pNode->mTransformation * parentTransform;

			if (pNode->mNumMeshes > 0)
			{
				aiVector3D   aiScale;
				aiQuaternion aiRotation;
				aiVector3D   aiPosition;
				worldTransform.Decompose(aiScale, aiRotation, aiPosition);
				outNodes.push_back({ pNode, { aiPosition.x, aiPosition.y, aiPosition.z }, { aiRotation.x, aiRotation.y, aiRotation.z, aiRotation.w }, aiScale });
			}

			for (dU32 i = 0; i < pNode->mNumChildren; i++)
				FlattenNodes(pNode->mChildren[i], worldTransform, outNodes);
		}

		dU32 ResolveImage(ImportContext& context, const aiMaterial* pMaterial, aiTextureType type)
		{
			aiString texturePath;
			if (pMaterial->GetTexture(type, 0, &texturePath) != aiReturn_SUCCESS)
				return kInvalidModelIndex;

			const dString path = context.directory + texturePath.C_Str();
			auto it = context.imageLookup.find(path);
			if (it != context.imageLookup.end())
				return it->second;

			const dU32 imageIndex = (dU32)context.pModel->images.size();
			context.pModel->images.push_back({ .source = FileSystem::Resolve<Image>(path.c_str()) });
			context.imageLookup.emplace(path, imageIndex);
			return imageIndex;
		}

		void ScaleMesh(ModelMesh& mesh, const aiVector3D& scale)
		{
			const dMatrix matrix = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
			const dMatrix normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, matrix));
			const bool isMirrored = scale.x * scale.y * scale.z < 0.0f;

			for (Vertex& vertex : mesh.vertices)
			{
				DirectX::XMStoreFloat3(&vertex.position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex.position), matrix));
				DirectX::XMStoreFloat3(&vertex.normal, DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&vertex.normal), normalMatrix)));
				const dVec tangent = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&vertex.tangent), matrix));
				DirectX::XMStoreFloat4(&vertex.tangent, DirectX::XMVectorSetW(tangent, isMirrored ? -vertex.tangent.w : vertex.tangent.w));
			}

			if (isMirrored)
			{
				for (dSizeT i = 0; i + 2 < mesh.indices.size(); i += 3)
					std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
			}
		}

		ModelMaterial ExtractMaterial(ImportContext& context, const aiMaterial* pMaterial)
		{
			ModelMaterial material{ .metalness = 1.0f, .roughness = 1.0f };
			material.albedoImage = ResolveImage(context, pMaterial, aiTextureType_BASE_COLOR);
			material.normalImage = ResolveImage(context, pMaterial, aiTextureType_NORMALS);
			material.roughnessMetalnessImage = ResolveImage(context, pMaterial, aiTextureType_DIFFUSE_ROUGHNESS);

			aiUVTransform data;
			if (pMaterial->Get(AI_MATKEY_BASE_COLOR, data) == aiReturn_SUCCESS)
				material.baseColor = *((dVec3*)&data);
			if (pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, data) == aiReturn_SUCCESS)
				material.roughness = *(float*)(&data);
			if (pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, data) == aiReturn_SUCCESS)
				material.metalness = *(float*)(&data);

			aiString alphaMode;
			if (pMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn_SUCCESS)
			{
				if (strcmp(alphaMode.C_Str(), "MASK") == 0)
					material.alphaMode = EAlphaMode::Mask;
				else if (strcmp(alphaMode.C_Str(), "BLEND") == 0)
					material.alphaMode = EAlphaMode::Blend;
			}

			float alphaCutoff;
			if (pMaterial->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == aiReturn_SUCCESS)
				material.alphaCutoff = alphaCutoff;

			int doubleSided;
			if (pMaterial->Get(AI_MATKEY_TWOSIDED, doubleSided) == aiReturn_SUCCESS && doubleSided != 0)
				material.faceCulling = RHI::ECullingMode::None;

			return material;
		}

		dU32 ResolveMaterial(ImportContext& context, dU32 aiMaterialIndex)
		{
			dU32& materialIndex = context.materialLookup[aiMaterialIndex];
			if (materialIndex == kInvalidModelIndex)
			{
				materialIndex = (dU32)context.pModel->materials.size();
				context.pModel->materials.push_back(ExtractMaterial(context, context.pScene->mMaterials[aiMaterialIndex]));
			}
			return materialIndex;
		}

		void ExtractNodeMesh(ImportContext& context, const NodeRef& ref, ModelMesh& outMesh, dVector<dU32>& outMaterials)
		{
			const aiNode* pNode = ref.pNode;
			dVector<dU32> aiMaterialIndices;
			outMesh.subMeshes.reserve(pNode->mNumMeshes);

			for (dU32 i = 0; i < pNode->mNumMeshes; i++)
			{
				const aiMesh* pMesh = context.pScene->mMeshes[pNode->mMeshes[i]];

				dU32 materialSlot;
				for (materialSlot = 0; materialSlot < (dU32)aiMaterialIndices.size(); materialSlot++)
					if (aiMaterialIndices[materialSlot] == pMesh->mMaterialIndex)
						break;
				if (materialSlot == aiMaterialIndices.size())
					aiMaterialIndices.push_back(pMesh->mMaterialIndex);

				SubMesh& subMesh = outMesh.subMeshes.emplace_back();
				subMesh.vertexOffset = (dU32)outMesh.vertices.size();
				subMesh.indexOffset = (dU32)outMesh.indices.size();
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
					}
					outMesh.vertices.push_back(vertex);
				}

				for (dU32 f = 0; f < pMesh->mNumFaces; f++)
				{
					const aiFace& face = pMesh->mFaces[f];
					if (face.mNumIndices != 3)
						continue;
					outMesh.indices.push_back(face.mIndices[0]);
					outMesh.indices.push_back(face.mIndices[1]);
					outMesh.indices.push_back(face.mIndices[2]);
				}
				subMesh.indexCount = (dU32)outMesh.indices.size() - subMesh.indexOffset;
			}
			outMesh.materialSlotCount = (dU32)aiMaterialIndices.size();

			if (ref.scale.x != 1.0f || ref.scale.y != 1.0f || ref.scale.z != 1.0f)
				ScaleMesh(outMesh, ref.scale);

			outMaterials.reserve(aiMaterialIndices.size());
			for (dU32 aiMaterialIndex : aiMaterialIndices)
				outMaterials.push_back(ResolveMaterial(context, aiMaterialIndex));
		}
	}

	bool Import(const char* filePath, ModelData& outModel)
	{
		const dString path = FileSystem::ResolvePath(filePath);

		Assimp::Importer importer;
		const aiScene* pScene{ importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace) };
		if (!pScene)
		{
			LOG_ERROR(StringUtils::printf("ModelImporter: %s (%s)", importer.GetErrorString(), path.c_str()).c_str());
			return false;
		}

		outModel = {};
		const dSizeT lastSlash = path.find_last_of("/\\");
		ImportContext context
		{
			.pScene = pScene,
			.directory = (lastSlash == dString::npos) ? dString() : path.substr(0, lastSlash + 1),
			.pModel = &outModel,
			.materialLookup = dVector<dU32>(pScene->mNumMaterials, kInvalidModelIndex),
		};

		dVector<NodeRef> nodeRefs;
		FlattenNodes(pScene->mRootNode, {}, nodeRefs);

		for (const NodeRef& ref : nodeRefs)
		{
			ModelMesh mesh;
			ModelNode node{ .position = ref.position, .rotation = ref.rotation };
			ExtractNodeMesh(context, ref, mesh, node.materials);
			if (node.materials.empty())
				continue;

			node.mesh = (dU32)outModel.meshes.size();
			outModel.meshes.push_back(std::move(mesh));
			outModel.nodes.push_back(std::move(node));
		}

		return true;
	}
}
