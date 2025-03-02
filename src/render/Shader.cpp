#include "stdafx.h"
#include "Shader.h"

#include <d3dcompiler.h>
#include <d3d9.h>

#include <magic_enum.hpp>
#include "Common.h"
#include "../Util.h"

namespace render {

	LPD3D10INCLUDE includeHandling = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	// optimization flag does affect neither runtime nor compile speed much or at all currently
#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL2;
#else
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	bool throwOnError(const HRESULT& hr) {
		return util::throwOnError(hr, "Shader Creation Error:");
		IDirect3DDevice9Ex* p;
	}

	enum class ShaderType {
		Vertex, Pixel, Compute
	};

	ID3D10Blob* compile(D3d d3d, const std::string& source, ShaderType type, const D3D_SHADER_MACRO* defines = 0)
	{
		LPCSTR profile = type == ShaderType::Vertex ? "vs_5_0" : type == ShaderType::Pixel ? "ps_5_0" : "cs_5_0";
		LPCSTR entry = type == ShaderType::Vertex ? "VS_Main" : type == ShaderType::Pixel ? "PS_Main" : "CS_Main";
		std::wstring sourceW = util::utf8ToWide(source);
		ID3D10Blob* result = nullptr;
		ID3D10Blob* error = nullptr;

		auto hr = D3DCompileFromFile(
			sourceW.c_str(), defines, includeHandling, entry, profile, compileFlags, 0, &result, &error);

		if (error != nullptr) {
			LOG(WARNING) << std::string((char*)error->GetBufferPointer());
		}
		util::throwOnError(hr, std::string(magic_enum::enum_name(type)) + " Shader Compilation Error:");
		return result;
	}

	ComputeShader createComputeShader(D3d d3d, const std::string& sourceFile)
	{
		const D3D_SHADER_MACRO defines[] =
		{
			"EXAMPLE_DEFINE", "1",
			NULL, NULL
		};
		ID3DBlob* compiled = compile(d3d, sourceFile, ShaderType::Compute, defines);

		ComputeShader result;
		d3d.device->CreateComputeShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), NULL, &result.computeShader);
		return result;
	}

	Shader::Shader(D3d d3d, const std::string& sourceFile, const std::vector<VertexInputLayoutDesc>& layoutDesc, bool vsOnly)
	{
		LOG(DEBUG) << "Compiling shader from file: " << sourceFile;

		// compile both shaders
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		ID3D10Blob* VS = compile(d3d, sourceFile, ShaderType::Vertex);
		ID3D10Blob* PS = nullptr;
		if (!vsOnly) {
			PS = compile(d3d, sourceFile, ShaderType::Pixel);
		}

		// encapsulate shader blobs into shader objects
		HRESULT hr;
		hr = d3d.device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &vertexShader);
		throwOnError(hr);
		if (!vsOnly) {
			hr = d3d.device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &pixelShader);
			throwOnError(hr);
		}

		// create full layout desc from given partial layout desc
		size_t length = layoutDesc.size();
		D3D11_INPUT_ELEMENT_DESC* d3d11LayoutDesc = new D3D11_INPUT_ELEMENT_DESC[length];
		for (size_t i = 0; i < length; i = i + 1) {
			d3d11LayoutDesc[i].SemanticName = layoutDesc[i].SemanticName;
			d3d11LayoutDesc[i].SemanticIndex = layoutDesc[i].SemanticIndex;
			d3d11LayoutDesc[i].Format = layoutDesc[i].Format;
			d3d11LayoutDesc[i].InputSlot = layoutDesc[i].InputSlot;
			d3d11LayoutDesc[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			d3d11LayoutDesc[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			d3d11LayoutDesc[i].InstanceDataStepRate = 0;
		}
		hr = d3d.device->CreateInputLayout(d3d11LayoutDesc, length, VS->GetBufferPointer(), VS->GetBufferSize(), &vertexLayout);
		throwOnError(hr);

		delete[] d3d11LayoutDesc;
		release(VS);
		release(PS);
	}

	Shader::~Shader()
	{
		release(vertexLayout);
		release(vertexShader);
		release(pixelShader);
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