#pragma once

#include "render/basic/Primitives.h"
#include "render/Dx.h"
#include "render/WinDx.h" // TODO remove

#include <dxgiformat.h>

namespace render::d3d
{
	// TODO move to cpp with implicit instantiation
	template<typename T>
	void createStructuredBuf(
		D3d d3d, ID3D11Buffer** target, uint32_t count, const T* const data, BufferUsage usage, bool writeUnordered = false)
	{
		// TODO should writeUnordered ALWAYS be true for all BUFFER_WRITE_GPU usages?

		// https://www.gamedev.net/forums/topic/709796-working-with-structuredbuffer-in-hlsl-directx-11/
		// https://github.com/walbourn/directx-sdk-samples/blob/main/BasicCompute11/BasicCompute11.cpp#L500

		release(*target);
		D3D11_BUFFER_DESC desc = {};

		desc.Usage = usage;
		if (usage == BufferUsage::READBACK) {
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		}
		else {
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		}
		if (writeUnordered) {
			assert(usage == BufferUsage::WRITE_GPU);
			desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		desc.StructureByteStride = sizeof(T);
		desc.ByteWidth = sizeof(T) * count;

		if (data) {
			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = data;

			d3d.device->CreateBuffer(&desc, &initialData, target);
		}
		else {
			d3d.device->CreateBuffer(&desc, nullptr, target);
		}
	}

	template<typename T>
	void createStructuredBuf(
		D3d d3d, ID3D11Buffer** target, const std::vector<T>* const data, BufferUsage usage = BufferUsage::IMMUTABLE, bool writeUnordered = false)
	{
		createStructuredBuf<T>(d3d, target, data->size(), data->data(), usage, writeUnordered);
	}

	template<typename T>
	void createStructuredBuf(
		D3d d3d, ID3D11Buffer** target, uint32_t count, BufferUsage usage, bool writeUnordered = false)
	{
		createStructuredBuf<T>(d3d, target, count, nullptr, usage, writeUnordered);
	}

	void createStructuredSrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Buffer* buffer);
	void createStructuredUav(D3d d3d, ID3D11UnorderedAccessView** targetUav, ID3D11Buffer* buffer);
	void createStructuredBufForReadbackCopy(D3d d3d, ID3D11Buffer** target, ID3D11Buffer* originalBuffer);
}