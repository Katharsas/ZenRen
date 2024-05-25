#pragma once

#include "RendererCommon.h"

namespace renderer::util {
	void dumpVerts(const std::string& matName, const std::vector<VERTEX_POS>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther);
	std::string getVramUsage(IDXGIAdapter3* adapter);

	// TODO this should probably go into pipelineutil
	template<typename T>
	void createVertexBuffer(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData)
	{
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));

		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = sizeof(T) * vertexData.size();

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = vertexData.data();
		d3d.device->CreateBuffer(&bufferDesc, &initialData, target);
	}

	// TODO this should probably go into pipelineutil
	template<typename T>
	void createConstantBuffer(D3d d3d, ID3D11Buffer** target, D3D11_USAGE usage)
	{
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

		bufferDesc.Usage = usage;
		bufferDesc.ByteWidth = sizeof(T);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
	}
}

