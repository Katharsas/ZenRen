#include "stdafx.h"
#include "Shader.h"

#include <d3dcompiler.h>
#include <d3d9.h>

#include "render/WinDx.h"
#include "Util.h"

#include "magic_enum.hpp"

namespace render::d3d
{
	LPD3D10INCLUDE includeHandling = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	// optimization flag does affect neither runtime nor compile speed much or at all currently
#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL2;
#else
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	const std::string folder = "shaders";
	const std::string extension = ".hlsl";

	void Shader::set(D3d d3d)
	{
		d3d.deviceContext->IASetInputLayout(vertexLayout);
		d3d.deviceContext->VSSetShader(vertexShader, 0, 0);
		if (pixelShader != nullptr) {
			d3d.deviceContext->PSSetShader(pixelShader, 0, 0);
		}
	}

	constexpr std::array typeToFormat = util::createExactCount<DXGI_FORMAT, magic_enum::enum_count<render::Type>()>({
			DXGI_FORMAT_R32_FLOAT,
			DXGI_FORMAT_R32G32_FLOAT,
			DXGI_FORMAT_R32G32B32_FLOAT,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			DXGI_FORMAT_R32_UINT,
			DXGI_FORMAT_R32G32_UINT,
	});

	const std::array semanticToString = util::createExactCount<std::string, magic_enum::enum_count<render::Semantic>()>({
		"POSITION",
		"NORMAL",
		"TEXCOORD",
		"COLOR",
		"INDEX",
		"OTHER"
	});
	
	DXGI_FORMAT toFormat(render::Type layout)
	{
		return typeToFormat.at((size_t)layout);
	}

	const std::string& toSemanticName(render::Semantic semantic)
	{
		return semanticToString.at((size_t) semantic);
	}

	std::vector<VertexAttributeDesc> buildInputLayoutDesc(const std::initializer_list<VertexAttributes>& buffers)
	{
		std::vector<VertexAttributeDesc> result;
		std::array<uint32_t, magic_enum::enum_count<render::Semantic>()> typeToCount = { { 0, 0, 0, 0 } };

		uint32_t bufferSlot = 0;

		for (auto& attributes : buffers) {
			for (auto& attr : attributes) {
				VertexAttributeDesc desc = {};
				desc.format = toFormat(attr.first);
				desc.bufferSlot = bufferSlot;
				desc.semanticName = &toSemanticName(attr.second);
				uint32_t& semanticIndex = typeToCount.at((uint32_t)attr.second);
				desc.semanticIndex = semanticIndex;
				semanticIndex++;

				result.push_back(desc);
			}
			bufferSlot++;
		}

		return result;
	}

	std::string filePath(const std::string& shaderName)
	{
		return folder + "/" + shaderName + extension;
	}

	bool throwOnError(const HRESULT& hr) {
		return util::throwOnError(hr, "Shader Creation Error:");
	}

	bool warnOnError(const HRESULT& hr) {
		return util::warnOnError(hr, "Shader Creation Error:");
	}

	bool createInputLayout(D3d d3d, ID3D11InputLayout** target, const std::vector<VertexAttributeDesc>& inputLayout, ID3D10Blob* vsCompiled)
	{
		std::vector<D3D11_INPUT_ELEMENT_DESC> layoutDesc = {};
		for (auto& vertexAttr : inputLayout) {
			D3D11_INPUT_ELEMENT_DESC desc;
			desc.SemanticName = vertexAttr.semanticName->c_str();
			desc.SemanticIndex = vertexAttr.semanticIndex;
			desc.Format = vertexAttr.format;
			desc.InputSlot = vertexAttr.bufferSlot;
			desc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			desc.InstanceDataStepRate = 0;
			layoutDesc.push_back(desc);
		}
		HRESULT hr = d3d.device->CreateInputLayout(
			layoutDesc.data(), layoutDesc.size(),
			vsCompiled->GetBufferPointer(), vsCompiled->GetBufferSize(),
			target);
		return warnOnError(hr);
	}

	enum class ShaderType {
		Vertex, Pixel, Compute
	};

	ID3D10Blob* compile(D3d d3d, const std::string& source, ShaderType type, bool isVertexInputPacked)
	{
		LPCSTR profile = type == ShaderType::Vertex ? "vs_5_0" : type == ShaderType::Pixel ? "ps_5_0" : "cs_5_0";
		LPCSTR entry = type == ShaderType::Vertex ? "VS_Main" : type == ShaderType::Pixel ? "PS_Main" : "CS_Main";
		std::wstring sourceW = util::utf8ToWide(source);
		ID3D10Blob* result = nullptr;
		ID3D10Blob* error = nullptr;

		const D3D_SHADER_MACRO defines[] = {
			{"VERTEX_INPUT_RAW", isVertexInputPacked ? "0" : "1" },
			{nullptr, nullptr},
		};
		auto hr = D3DCompileFromFile(
			sourceW.c_str(), defines, includeHandling, entry, profile, compileFlags, 0, &result, &error);

		if (error != nullptr) {
			LOG(WARNING) << std::string((char*)error->GetBufferPointer());
		}
		util::warnOnError(hr, std::string(magic_enum::enum_name(type)) + " Shader Compilation Error:");
		return result;
	}

	void createShader(D3d d3d, Shader& target, const std::string& shaderName, const std::vector<VertexAttributeDesc>& inputLayout, bool vsOnly)
	{
		bool success = true;

		std::string sourceFile = filePath(shaderName);
		LOG(DEBUG) << "Compiling shader from file: " << sourceFile;

		// compile both shaders
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		ID3D10Blob* VS = compile(d3d, sourceFile, ShaderType::Vertex, PACK_VERTEX_ATTRIBUTES);
		success = VS != nullptr && success;
		ID3D10Blob* PS = nullptr;
		if (!vsOnly) {
			PS = compile(d3d, sourceFile, ShaderType::Pixel, PACK_VERTEX_ATTRIBUTES);
			success = PS != nullptr && success;
		}

		// encapsulate shader blobs into shader objects
		Shader result;
		if (success) {
			HRESULT hr;
			hr = d3d.device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &result.vertexShader);
			success = warnOnError(hr) && success;
			if (!vsOnly) {
				hr = d3d.device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &result.pixelShader);
				success = warnOnError(hr) && success;
			}

			success = createInputLayout(d3d, &result.vertexLayout, inputLayout, VS) && success;
		}
		
		release(VS);
		release(PS);

		if (!success && target.vertexShader == nullptr) {
			// during first start we actually error out if shaders are bad
			util::throwError("Failed to initialize shaders.");
		}
		// late assignment so shader loading exception does not cause broken shaders
		if (success) {
			target.release();
			target = result;
		}
		else {
			// discard result and keep old shaders
			result.release();
		}
	}

	void createComputeShader(D3d d3d, ID3D11ComputeShader** target, const std::string& shaderName)
	{
		std::string sourceFile = filePath(shaderName);
		const D3D_SHADER_MACRO defines[] =
		{
			"EXAMPLE_DEFINE", "1",
			NULL, NULL
		};
		ID3DBlob* compiled = compile(d3d, sourceFile, ShaderType::Compute, defines);

		d3d.device->CreateComputeShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), NULL, target);
	}
}