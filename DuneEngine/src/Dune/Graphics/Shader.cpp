#include "pch.h"
#include "Shader.h"
#include "Renderer.h"

namespace Dune
{
	Shader::Shader(const ShaderDesc& desc)
	{
		Renderer& renderer{ Renderer::GetInstance() };
		ID3D12Device* pDevice{ renderer.GetDevice() };

		Microsoft::WRL::ComPtr<IDxcLibrary> pLibrary;
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));

		Microsoft::WRL::ComPtr<IDxcCompiler> pCompiler{ nullptr };
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

		Microsoft::WRL::ComPtr<IDxcUtils> pUtils{ nullptr };
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));

		Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler{ nullptr };
		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

		dU32 codePage{ CP_UTF8 };
		const wchar_t* args[] 
		{  
			L"-I", SHADER_DIR
		};

		dWString vertexShaderPath{ SHADER_DIR };
		vertexShaderPath.append(desc.VS.fileName);

		dWString pixelShaderPath{ SHADER_DIR };
		pixelShaderPath.append(desc.PS.fileName);

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSourceBlob{ nullptr };
		ThrowIfFailed( pLibrary->CreateBlobFromFile(vertexShaderPath.c_str(), &codePage, &pSourceBlob));
		Microsoft::WRL::ComPtr<IDxcOperationResult> pResult{nullptr};
		ThrowIfFailed(pCompiler->Compile(
			pSourceBlob.Get(),		// pSource
			desc.VS.fileName,		// pSourceName
			desc.VS.entryFunc,		// pEntryPoint
			L"vs_6_6",				// pTargetProfile
			args, _countof(args),	// pArguments, argCount
			NULL, 0,				// pDefines, defineCount
			pIncludeHandler.Get(),	// pIncludeHandler
			&pResult));				// ppResult

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pErrorsBlob{ nullptr };
		if (SUCCEEDED(pResult->GetErrorBuffer(&pErrorsBlob)) && pErrorsBlob)
		{
			OutputDebugStringA((const char*)pErrorsBlob->GetBufferPointer());
		}

		Microsoft::WRL::ComPtr<IDxcBlob> VSBlob{ nullptr };
		pResult->GetResult(&VSBlob);

		pSourceBlob->Release();
		pResult->Release();
		ThrowIfFailed(pLibrary->CreateBlobFromFile(pixelShaderPath.c_str(), &codePage, &pSourceBlob));
		ThrowIfFailed(pCompiler->Compile(
			pSourceBlob.Get(),		// pSource
			desc.PS.fileName,		// pSourceName
			desc.PS.entryFunc,		// pEntryPoint
			L"ps_6_6",				// pTargetProfile
			args, _countof(args),	// pArguments, argCount
			NULL, 0,				// pDefines, defineCount
			pIncludeHandler.Get(),	// pIncludeHandler
			&pResult));			// ppResult

		pErrorsBlob->Release();
		if (SUCCEEDED(pResult->GetErrorBuffer(&pErrorsBlob)) && pErrorsBlob.Get())
		{
			OutputDebugStringA((const char*)pErrorsBlob->GetBufferPointer());
		}

		Microsoft::WRL::ComPtr<IDxcBlob> PSBlob{ nullptr };
		pResult->GetResult(&PSBlob);

		CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[6];
		// Global constant (Camera matrices)
		rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL);
		// Instances Data
		rootParameters[1].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_VERTEX);
		// Point light buffers
		rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		// Directional light buffers
		rootParameters[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		// Shadow maps
		rootParameters[4].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		// Sampler
		rootParameters[5].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		);

		Microsoft::WRL::ComPtr<ID3DBlob> signature{ nullptr };
		Microsoft::WRL::ComPtr<ID3DBlob> error{ nullptr };
		if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error)))
		{
			OutputDebugStringA((const char*)error->GetBufferPointer());
			Assert(0);
		}
		ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_pRootSignature;
		psoDesc.VS.BytecodeLength = VSBlob->GetBufferSize();
		psoDesc.VS.pShaderBytecode = VSBlob->GetBufferPointer();
		psoDesc.PS.BytecodeLength = PSBlob->GetBufferSize();
		psoDesc.PS.pShaderBytecode = PSBlob->GetBufferPointer();
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
	}

	Shader::~Shader()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseResource(m_pPipelineState);
		renderer.ReleaseResource(m_pRootSignature);
	}
}
