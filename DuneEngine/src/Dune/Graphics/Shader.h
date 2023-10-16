#pragma once

namespace Dune
{
	template <typename T, typename H>
	class Pool;

	struct ShaderCode
	{
		const wchar_t* fileName = nullptr;
		const wchar_t* entryFunc = nullptr;
	};

	struct DepthStencilDesc
	{
		bool depthEnable = false;
		bool depthWrite = false;
		D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		bool stencilEnable = false;
	};

	struct ShaderDesc
	{
		const char* debugName = nullptr;
		
		ShaderCode VS;
		ShaderCode PS;
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		DepthStencilDesc depthStencilDesc;
	};

	class Shader
	{
	public:
		ID3D12RootSignature* GetRootSignature() const { return m_pRootSignature; }
		ID3D12PipelineState* GetPSO() const { return m_pPipelineState; }

	private:
		Shader(const ShaderDesc& desc);
		~Shader();
		DISABLE_COPY_AND_MOVE(Shader);

	private:
		friend Pool<Shader, Shader>;

		ID3D12RootSignature* m_pRootSignature;
		ID3D12PipelineState* m_pPipelineState;
	};
}