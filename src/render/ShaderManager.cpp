#include "stdafx.h"
#include "ShaderManager.h"

#include <stdexcept>

namespace render {

	ShaderManager::ShaderManager(D3d d3d)
	{
		reloadShaders(d3d);
	}


	ShaderManager::~ShaderManager()
	{
		clearAll();
	}


	void ShaderManager::reloadShaders(D3d d3d) {
		LOG(INFO) << "Reloading all shaders.";
		clearAll();
		{
			std::string shaderName("toneMapping");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		} {
			std::string shaderName("renderToTexture");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		} {
			std::string shaderName("forward/depthPrepass");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT }
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc, true);
		} {
			std::string shaderName("forward/main");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1 },
				{ "NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 1 },
				//{ "ENUM", 0, DXGI_FORMAT_R8_UINT, 1 },
				{ "TEXCOORD", 3, DXGI_FORMAT_R32_UINT, 2 },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		} {
			std::string shaderName("forward/mainTexColorOnly");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1 },
				{ "NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1 },
				{ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 1 },
				{ "TEXCOORD", 3, DXGI_FORMAT_R32_UINT, 2 },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		} {
			std::string shaderName("forward/wireframe");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		} {
			std::string shaderName("forward/sky");
			std::vector<VertexInputLayoutDesc> layoutDesc =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 1 },
			};
			shaders[shaderName] = new Shader(d3d, filePath(shaderName), layoutDesc);
		}
	}

	Shader * ShaderManager::getShader(const std::string& shaderName)
	{
		auto resultPair = shaders.find(shaderName);
		if (resultPair == shaders.end()) {
			throw std::logic_error("There is no shader with name " + shaderName + " !");
		}
		return resultPair->second;
	}

	void ShaderManager::insert(const std::string& shaderName, Shader* shader)
	{
		auto resultPair = shaders.insert(std::make_pair(shaderName, shader));
		if (resultPair.second == false) {
			throw std::logic_error("There already exists a shader with name " + shaderName + " !");
		}
	}

	void ShaderManager::clearAll()
	{
		for (auto& pair : shaders) {
			delete pair.second;
		}
		shaders.clear();
	}

	const std::string ShaderManager::filePath(const std::string shaderName)
	{
		return folder + "/" + shaderName + extension;
	}

}