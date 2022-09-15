#include "stdafx.h"
#include "ShaderManager.h"

#include <stdexcept>

namespace renderer {

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
			std::string shaderName(u8"testTriangle");
			Shader::VertexInputLayoutDesc layoutDesc[] =
			{
				{ "POSITION", DXGI_FORMAT_R32G32B32_FLOAT },
				{ "COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT },
			};
			shaders[shaderName] = new Shader(filePath(shaderName), layoutDesc, std::size(layoutDesc), d3d);
		} {
			std::string shaderName(u8"toneMapping");
			Shader::VertexInputLayoutDesc layoutDesc[] =
			{
				{ "POSITION", DXGI_FORMAT_R32G32B32_FLOAT },
				{ "TEXCOORD", DXGI_FORMAT_R32G32_FLOAT },
			};
			shaders[shaderName] = new Shader(filePath(shaderName), layoutDesc, std::size(layoutDesc), d3d);
		} {
			std::string shaderName(u8"flatBasicColorTexShader");
			Shader::VertexInputLayoutDesc layoutDesc[] =
			{
				{ "POSITION", DXGI_FORMAT_R32G32B32_FLOAT },
				{ "NORMAL", DXGI_FORMAT_R32G32B32_FLOAT },
				{ "TEXCOORD", DXGI_FORMAT_R32G32_FLOAT },
				{ "COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT },
				//{ "LIGHT_INTENSITY", DXGI_FORMAT_R32_FLOAT },
				
			};
			shaders[shaderName] = new Shader(filePath(shaderName), layoutDesc, std::size(layoutDesc), d3d);
		}
	}

	Shader * ShaderManager::getShader(const std::string& shaderName)
	{
		auto resultPair = shaders.find(shaderName);
		if (resultPair == shaders.end()) {
			throw std::logic_error(u8"There is no shader with name " + shaderName + u8" !");
		}
		return resultPair->second;
	}

	void ShaderManager::insert(const std::string& shaderName, Shader* shader)
	{
		auto resultPair = shaders.insert(std::make_pair(shaderName, shader));
		if (resultPair.second == false) {
			throw std::logic_error(u8"There already exists a shader with name " + shaderName + u8" !");
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
		return folder + u8"/" + shaderName + extension;
	}

}