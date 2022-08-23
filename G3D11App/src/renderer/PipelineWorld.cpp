#include "stdafx.h"
#include "PipelineWorld.h"

#include "Camera.h"
#include "Texture.h"

namespace renderer::world {

	struct CbPerObject
	{
		XMMATRIX worldViewProjection;
	};

	ID3D11Buffer* cbPerObjectBuffer;

	MeshIndexed cube;

	// scene state
	float rot = 0.1f;
	XMMATRIX cube1World;
	XMMATRIX cube2World;

	Texture* texture;
	ID3D11SamplerState* linearSamplerState = nullptr;

	void updateObjects()
	{
		//Keep the cubes rotating
		rot += .015f;
		if (rot > 6.28f) rot = 0.0f;

		cube1World = XMMatrixIdentity();
		XMVECTOR rotAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMMATRIX Rotation = XMMatrixRotationAxis(rotAxis, rot);
		XMMATRIX Translation = XMMatrixTranslation(0.0f, 0.0f, 4.0f);
		cube1World = Translation * Rotation;

		cube2World = XMMatrixIdentity();
		Rotation = XMMatrixRotationAxis(rotAxis, -rot);
		XMMATRIX Scale = XMMatrixScaling(1.3f, 1.3f, 1.3f);
		cube2World = /*Rotation * */Scale;
	}

	void initLinearSampler(D3d d3d, RenderSettings settings)
	{
		// TODO Mip Maps

		if (linearSamplerState != nullptr) {
			linearSamplerState->Release();
		}
		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));

		if (settings.anisotropicFilter) {
			samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		} else {
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = settings.anisotropicLevel;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		d3d.device->CreateSamplerState(&samplerDesc, &linearSamplerState);
	}

	void initVertexIndexBuffers(D3d d3d, bool reverseZ)
	{
		texture = new Texture(d3d, "BARRIERE.png");

		std::array<POS_UV, 24> cubeVerts = { {
			{ POS(-1.0f, -1.0f, -1.0f), UV(0.0f, 0.0f) },
			{ POS(-1.0f, +1.0f, -1.0f), UV(1.0f, 0.0f) },
			{ POS(+1.0f, +1.0f, -1.0f), UV(1.0f, 1.0f) },
			{ POS(+1.0f, -1.0f, -1.0f), UV(0.0f, 1.0f) },

			{ POS(-1.0f, -1.0f, +1.0f), UV(0.0f, 0.0f) },
			{ POS(-1.0f, +1.0f, +1.0f), UV(1.0f, 0.0f) },
			{ POS(+1.0f, +1.0f, +1.0f), UV(1.0f, 1.0f) },
			{ POS(+1.0f, -1.0f, +1.0f), UV(0.0f, 1.0f) },

			{ POS(-1.0f, -1.0f, -1.0f), UV(0.0f, 0.0f) },
			{ POS(-1.0f, -1.0f, +1.0f), UV(1.0f, 0.0f) },
			{ POS(+1.0f, -1.0f, +1.0f), UV(1.0f, 1.0f) },
			{ POS(+1.0f, -1.0f, -1.0f), UV(0.0f, 1.0f) },

			{ POS(-1.0f, +1.0f, -1.0f), UV(0.0f, 0.0f) },
			{ POS(-1.0f, +1.0f, +1.0f), UV(1.0f, 0.0f) },
			{ POS(+1.0f, +1.0f, +1.0f), UV(1.0f, 1.0f) },
			{ POS(+1.0f, +1.0f, -1.0f), UV(0.0f, 1.0f) },

			{ POS(-1.0f, -1.0f, -1.0f), UV(0.0f, 0.0f) },
			{ POS(-1.0f, -1.0f, +1.0f), UV(1.0f, 0.0f) },
			{ POS(-1.0f, +1.0f, +1.0f), UV(1.0f, 1.0f) },
			{ POS(-1.0f, +1.0f, -1.0f), UV(0.0f, 1.0f) },

			{ POS(+1.0f, -1.0f, -1.0f), UV(0.0f, 0.0f) },
			{ POS(+1.0f, -1.0f, +1.0f), UV(1.0f, 0.0f) },
			{ POS(+1.0f, +1.0f, +1.0f), UV(1.0f, 1.0f) },
			{ POS(+1.0f, +1.0f, -1.0f), UV(0.0f, 1.0f) },
		} };
		// make sure rotation of each triangle is clockwise
		std::array<DWORD, 36> cubeIndices = { {
				// front
				 0, 1, 2, 0, 2, 3,
				// back
				 4, 5, 6, 4, 6, 7,
				// left
				 8, 9,10, 8,10,11,
				// right
				12,13,14,12,14,15,
				// top
				16,17,18,16,18,19,
				// bottom
				20,21,22,20,22,23
			} };

		/*
		const float zNear = reverseZ ? 1.0f : 0.0f;
		std::array<VERTEX, 3> triangleData = { {
			{ POS(+0.0f, +0.5f, zNear), D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f) },
			{ POS(+0.45f, -0.5f, zNear), D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f) },
			{ POS(-0.45f, -0.5f, zNear), D3DXCOLOR(0.0f, 0.0f, 1.0f, 1.0f) },
		} };

		// vertex buffer triangle (dynamic, mappable)
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DYNAMIC;				// write access by CPU and read access by GPU
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// use as a vertex buffer
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// allow CPU to write in buffer
			bufferDesc.ByteWidth = sizeof(VERTEX) * triangleData.size();

			// see https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ne-d3d11-d3d11_usage for cpu/gpu data exchange
			d3d.device->CreateBuffer(&bufferDesc, nullptr, &(triangle.vertexBuffer));
			triangle.vertexCount = triangleData.size();
		}
		// map to copy triangle vertices into its buffer
		{
			D3D11_MAPPED_SUBRESOURCE ms;
			d3d.deviceContext->Map(triangle.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);// map the buffer
			memcpy(ms.pData, triangleData.data(), sizeof(triangleData));									// copy the data
			d3d.deviceContext->Unmap(triangle.vertexBuffer, 0);											// unmap the buffer
		}
		*/

		// vertex buffer cube
		{
			D3D11_BUFFER_DESC bufferDesc = CD3D11_BUFFER_DESC();
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(POS_UV) * cubeVerts.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = cubeVerts.data();
			d3d.device->CreateBuffer(&bufferDesc, &initialData, &(cube.vertexBuffer));
		}
		// index buffer cube
		{
			D3D11_BUFFER_DESC indexBufferDesc;
			ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.ByteWidth = sizeof(DWORD) * cubeIndices.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = cubeIndices.data();
			d3d.device->CreateBuffer(&indexBufferDesc, &initialData, &(cube.indexBuffer));
			cube.indexCount = cubeIndices.size();
		}

		// select which primtive type we are using
		d3d.deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void initConstantBufferPerObject(D3d d3d)
	{
		D3D11_BUFFER_DESC cbbd;
		ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

		cbbd.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		cbbd.ByteWidth = sizeof(CbPerObject);
		cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbbd.CPUAccessFlags = 0;
		cbbd.MiscFlags = 0;

		d3d.device->CreateBuffer(&cbbd, nullptr, &cbPerObjectBuffer);
	}

	void draw(D3d d3d, ShaderManager* shaders)
	{
		// set the shader objects avtive
		Shader* shader = shaders->getShader("flatBasicColorTexShader");
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		auto* resourceView = texture->GetResourceView();
		d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);
		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// select which vertex / index buffer to display
		UINT stride = sizeof(POS_UV);
		UINT offset = 0;
		d3d.deviceContext->IASetVertexBuffers(0, 1, &(cube.vertexBuffer), &stride, &offset);
		d3d.deviceContext->IASetIndexBuffer(cube.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// set the World/View/Projection matrix, send it to constant buffer, draw to linear buffer
		// cube 1
		CbPerObject cbPerObject = { camera::calculateWorldViewProjection(cube1World) };
		d3d.deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

		d3d.deviceContext->DrawIndexed(cube.indexCount, 0, 0);

		// cube 2
		cbPerObject = { camera::calculateWorldViewProjection(cube2World) };
		d3d.deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

		d3d.deviceContext->DrawIndexed(cube.indexCount, 0, 0);
	}

	void clean()
	{
		cbPerObjectBuffer->Release();
		linearSamplerState->Release();
		delete texture;

		cube.release();
	}
}