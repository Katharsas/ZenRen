#include "stdafx.h"
#include "Shader.h"

#include <d3dcompiler.h>

#include "../Util.h"

namespace renderer {

	Shader::Shader(const std::string& sourceFile, const VertexInputLayoutDesc layoutDesc[], const int length, D3d d3d)
	{
		LOG(DEBUG) << "Compiling shader from file: " << sourceFile;

		// compile both shaders
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		ID3D10Blob* VS, *PS, *errVS, *errPS;
		UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		auto hrVs = D3DX11CompileFromFileW(sourceFileW.c_str(), 0, 0, "VS_Main", "vs_5_0", flags, 0, 0, &VS, &errVS, 0);
		util::warnOnError(hrVs, "Vertex Shader Compilation Error:");
		auto hrPs = D3DX11CompileFromFileW(sourceFileW.c_str(), 0, 0, "PS_Main", "ps_5_0", flags, 0, 0, &PS, &errPS, 0);
		util::warnOnError(hrPs, "Pixel Shader Compilation Error:");

		if (errVS != nullptr) {
			LOG(WARNING) << std::string((char*)errVS->GetBufferPointer());
		}
		if (errPS != nullptr) {
			LOG(WARNING) << std::string((char*)errPS->GetBufferPointer());
		}
		// TODO if shader compilation fails, debugger might stop on exception before errors are flushed to log

		// encapsulate shader blobs into shader objects
		d3d.device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &vertexShader);
		d3d.device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &pixelShader);

		// create full layout desc from given partial layout desc
		D3D11_INPUT_ELEMENT_DESC * d3d11LayoutDesc = new D3D11_INPUT_ELEMENT_DESC[length];
		for (int i = 0; i < length; i = i + 1) {
			d3d11LayoutDesc[i].SemanticName = layoutDesc[i].SemanticName;
			d3d11LayoutDesc[i].SemanticIndex = layoutDesc[i].SemanticIndex;
			d3d11LayoutDesc[i].Format = layoutDesc[i].Format;
			d3d11LayoutDesc[i].InputSlot = 0;
			d3d11LayoutDesc[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			d3d11LayoutDesc[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			d3d11LayoutDesc[i].InstanceDataStepRate = 0;
		}
		d3d.device->CreateInputLayout(d3d11LayoutDesc, length, VS->GetBufferPointer(), VS->GetBufferSize(), &vertexLayout);

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

	ID3D11InputLayout* Shader::getVertexLayout()
	{
		return vertexLayout;
	}

	ID3D11VertexShader* Shader::getVertexShader()
	{
		return vertexShader;
	}

	ID3D11PixelShader* Shader::getPixelShader()
	{
		return pixelShader;
	}

}