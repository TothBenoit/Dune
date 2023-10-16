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

		IDxcBlob* pVSBlob{ nullptr };
		IDxcBlob* pPSBlob{ nullptr };

		dU32 codePage{ CP_UTF8 };
		const wchar_t* args[] 
		{  
			L"-I", SHADER_DIR, L"-Zi"
		};

		dWString vertexShaderPath{ SHADER_DIR };
		vertexShaderPath.append(desc.VS.fileName);

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

		pResult->GetResult(&pVSBlob);

		if (desc.PS.fileName)
		{
			pSourceBlob->Release();
			pResult->Release();

			dWString pixelShaderPath{ SHADER_DIR };
			pixelShaderPath.append(desc.PS.fileName);

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

			pResult->GetResult(&pPSBlob);
		}

		Microsoft::WRL::ComPtr<ID3DBlob> signature{ nullptr };
		Microsoft::WRL::ComPtr<ID3DBlob> error{ nullptr };
		if (FAILED(D3D12SerializeVersionedRootSignature(&desc.rootSignatureDesc, &signature, &error)))
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
		psoDesc.VS.BytecodeLength = pVSBlob->GetBufferSize();
		psoDesc.VS.pShaderBytecode = pVSBlob->GetBufferPointer();
		psoDesc.PS.BytecodeLength = (pPSBlob) ? pPSBlob->GetBufferSize() : 0;
		psoDesc.PS.pShaderBytecode = (pPSBlob) ? pPSBlob->GetBufferPointer() : nullptr;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = desc.depthStencilDesc.depthEnable;
		psoDesc.DepthStencilState.DepthWriteMask = (desc.depthStencilDesc.depthWrite) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = desc.depthStencilDesc.depthFunc;
		psoDesc.DepthStencilState.StencilEnable = desc.depthStencilDesc.stencilEnable;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D16_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));

		pVSBlob->Release();
		if (pPSBlob)
			pPSBlob->Release();
	}

	Shader::~Shader()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseResource(m_pPipelineState);
		renderer.ReleaseResource(m_pRootSignature);
	}
}
