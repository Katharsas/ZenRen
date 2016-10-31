#pragma once

#include "Shader.h"

class ShaderManager
{
public:
	ShaderManager(ID3D11Device *device);
	~ShaderManager();
	void reloadShaders(ID3D11Device* device);
	Shader* getShader(const std::string& shaderName);
private:
	std::unordered_map<std::string, Shader*> shaders;

	void insert(const std::string& shaderName, Shader* shader);

	void clearAll();

	const std::string folder = std::string(u8"Shaders");
	const std::string extension = std::string(u8".hlsl");

	const std::string filePath(const std::string shaderName);
};