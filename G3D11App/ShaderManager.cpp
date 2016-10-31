#include "stdafx.h"
#include "ShaderManager.h"

#include <stdexcept>


ShaderManager::ShaderManager(ID3D11Device* device)
{
	reloadShaders(device);
}


ShaderManager::~ShaderManager()
{
	clearAll();
}


void ShaderManager::reloadShaders(ID3D11Device* device) {
	LOG(INFO) << "Reloading all shaders.";
	clearAll();
	{
		std::string shaderName(u8"testTriangle");
		Shader::VertexInputLayoutDesc layoutDesc[] =
		{
			{ "POSITION", DXGI_FORMAT_R32G32B32_FLOAT },
			{ "COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT },
		};
		shaders[shaderName] = new Shader(filePath(shaderName), layoutDesc, 2, device);
	}{
		std::string shaderName(u8"toneMapping");
		Shader::VertexInputLayoutDesc layoutDesc[] =
		{
			{ "POSITION", DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD", DXGI_FORMAT_R32G32_FLOAT },
		};
	
		shaders[shaderName] = new Shader(filePath(shaderName), layoutDesc, 2, device);
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
