#pragma once

namespace Dune
{
	template <typename T, typename H>
	class Pool;

	struct ShaderDesc
	{
		const char* debugName = nullptr;
		
		Microsoft::WRL::ComPtr<ID3DBlob> VS;
		Microsoft::WRL::ComPtr<ID3DBlob> PS;
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