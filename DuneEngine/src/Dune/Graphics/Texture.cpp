#include "pch.h"
#include "Texture.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune
{
	Texture::Texture(const TextureDesc& desc)
		: m_usage{ desc.usage }
		, m_dimensions{ desc.dimensions[0], desc.dimensions[1], desc.dimensions[2] }
	{
		Renderer& renderer{ Renderer::GetInstance() };

		D3D12_RESOURCE_FLAGS resourceFlag;
		switch (m_usage)
		{
			case ETextureUsage::RTV:
				resourceFlag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				break;
			case ETextureUsage::DSV:
				resourceFlag = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				break;
			default:
				Assert(false);
				break;
		}

		CD3DX12_RESOURCE_DESC textureResourceDesc(
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			m_dimensions[0],
			m_dimensions[1],
			m_dimensions[2],
			1,
			desc.format,
			1,
			0,
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			resourceFlag);

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		ID3D12Device* pDevice{ renderer.GetDevice() };

		ThrowIfFailed(pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureResourceDesc,
			desc.state,
			&desc.clearValue,
			IID_PPV_ARGS(&m_texture)));
		NameDXObject(m_texture, desc.debugName);

		switch (m_usage)
		{
		case ETextureUsage::RTV:
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{ };
			rtvDesc.Format = desc.format;
			m_RTV = new DescriptorHandle[m_dimensions[2]];

			if (m_dimensions[2] > 1)
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.ArraySize = 1;
				rtvDesc.Texture2DArray.MipSlice = 0;
				rtvDesc.Texture2DArray.PlaneSlice = 0;

				for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
				{
					rtvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
					m_RTV[i] = renderer.m_rtvHeap.Allocate();
					pDevice->CreateRenderTargetView(m_texture, &rtvDesc, m_RTV[i].cpuAdress);
				}
			}
			else
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtvDesc.Texture2D.PlaneSlice = 0;
				rtvDesc.Texture2D.MipSlice = 0;
				m_RTV[0] = renderer.m_rtvHeap.Allocate();
				pDevice->CreateRenderTargetView(m_texture, &rtvDesc, m_RTV[0].cpuAdress);
			}

			break;
		}
		case ETextureUsage::DSV:
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = desc.format;
			m_DSV = new DescriptorHandle[m_dimensions[2]];

			if (m_dimensions[2] > 1)
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.ArraySize = 1;
				dsvDesc.Texture2DArray.MipSlice = 0;

				for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
				{
					dsvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
					m_DSV[i] = renderer.m_dsvHeap.Allocate();
					pDevice->CreateDepthStencilView(m_texture, &dsvDesc, m_DSV[i].cpuAdress);
				}
			}
			else
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Texture2D.MipSlice = 0;
				m_DSV[0] = renderer.m_dsvHeap.Allocate();
				pDevice->CreateDepthStencilView(m_texture, &dsvDesc, m_DSV[0].cpuAdress);
			}

			break;
		}
		default:
			Assert(false);
			break;
		}

		m_SRV = renderer.m_srvHeap.Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

		srvDesc.Format = (m_usage == ETextureUsage::DSV) ? (DXGI_FORMAT)(desc.format + 1) : desc.format;

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (m_dimensions[2] > 1)
		{
			srvDesc.Texture2DArray.ArraySize = m_dimensions[2];
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		}
		else
		{
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		}
		
		pDevice->CreateShaderResourceView(m_texture, &srvDesc, m_SRV.cpuAdress);
	}

	Texture::~Texture()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseResource(m_texture);
		renderer.m_srvHeap.Free(m_SRV);
		if (m_usage == ETextureUsage::RTV)
		{
			for (dU32 i = 0; i < m_dimensions[2]; i++)
				renderer.m_rtvHeap.Free(m_RTV[i]);
			delete[] m_RTV;
		}
		else if (m_usage == ETextureUsage::DSV)
		{
			for (dU32 i = 0; i < m_dimensions[2]; i++)
				renderer.m_dsvHeap.Free(m_DSV[i]);
			delete[] m_DSV;
		}
	}
}