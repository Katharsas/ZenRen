#include "stdafx.h"
#include "Shader.h"

#include "../Util.h"

namespace renderer {

	Shader::Shader(const std::string& sourceFile, const VertexInputLayoutDesc layoutDesc[],
		const int length, ID3D11Device *device)
	{
		LOG(DEBUG) << "Compiling shader from file: " << sourceFile;

		// compile both shaders
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		ID3D10Blob* VS, *PS, *errVS, *errPS;
		D3DX11CompileFromFileW(sourceFileW.c_str(), 0, 0, "VS_Main", "vs_4_0", 0, 0, 0, &VS, &errVS, 0);
		D3DX11CompileFromFileW(sourceFileW.c_str(), 0, 0, "PS_Main", "ps_4_0", 0, 0, 0, &PS, &errPS, 0);

		if (errVS != nullptr) {
			LOG(WARNING) << std::string((char*)errVS->GetBufferPointer());
		}
		if (errPS != nullptr) {
			LOG(WARNING) << std::string((char*)errPS->GetBufferPointer());
		}

		// encapsulate shader blobs into shader objects
		device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &vertexShader);
		device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &pixelShader);

		// create full layout desc from given partial layout desc
		D3D11_INPUT_ELEMENT_DESC * d3d11LayoutDesc = new D3D11_INPUT_ELEMENT_DESC[length];
		for (int i = 0; i < length; i = i + 1) {
			d3d11LayoutDesc[i].SemanticName = layoutDesc[i].SemanticName;
			d3d11LayoutDesc[i].SemanticIndex = 0;
			d3d11LayoutDesc[i].Format = layoutDesc[i].Format;
			d3d11LayoutDesc[i].InputSlot = 0;
			d3d11LayoutDesc[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			d3d11LayoutDesc[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			d3d11LayoutDesc[i].InstanceDataStepRate = 0;
		}
		device->CreateInputLayout(
			d3d11LayoutDesc, length, VS->GetBufferPointer(), VS->GetBufferSize(), &vertexLayout);

		delete[] d3d11LayoutDesc;
		VS->Release();
		PS->Release();
	}

	Shader::~Shader()
	{
		vertexLayout->Release();
		vertexShader->Release();
		pixelShader->Release();
	}

	ID3D11InputLayout * Shader::getVertexLayout()
	{
		return vertexLayout;
	}

	ID3D11VertexShader * Shader::getVertexShader()
	{
		return vertexShader;
	}

	ID3D11PixelShader * Shader::getPixelShader()
	{
		return pixelShader;
	}

}